// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/fileapi/external_file_url_loader_factory.h"

#include <algorithm>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/fileapi/external_file_resolver.h"
#include "chrome/browser/ash/fileapi/external_file_url_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/api/file_handlers/mime_util.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/base/io_buffer.h"
#include "net/http/http_byte_range.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "storage/browser/file_system/file_stream_reader.h"
#include "storage/browser/file_system/file_system_backend.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/file_system/isolated_context.h"

namespace ash {
namespace {

constexpr size_t kDefaultPipeSize = 65536;

// An IOBuffer that doesn't own its data and accepts void* pointers.
class MojoPipeIOBuffer : public net::IOBuffer {
 public:
  MojoPipeIOBuffer(void* data, size_t size)
      : net::IOBuffer(base::make_span(static_cast<char*>(data), size)) {}

  MojoPipeIOBuffer(const MojoPipeIOBuffer&) = delete;
  MojoPipeIOBuffer& operator=(const MojoPipeIOBuffer&) = delete;

 protected:
  ~MojoPipeIOBuffer() override = default;
};

// A helper class to read data from a FileStreamReader, and write it to a
// Mojo data pipe.
class FileSystemReaderDataPipeProducer {
 public:
  FileSystemReaderDataPipeProducer(
      mojo::ScopedDataPipeProducerHandle producer_handle,
      std::unique_ptr<storage::FileStreamReader> stream_reader,
      int remaining_bytes,
      base::OnceCallback<void(net::Error)> callback)
      : producer_handle_(std::move(producer_handle)),
        stream_reader_(std::move(stream_reader)),
        remaining_bytes_(remaining_bytes),
        total_bytes_written_(0),
        pipe_watcher_(std::make_unique<mojo::SimpleWatcher>(
            FROM_HERE,
            mojo::SimpleWatcher::ArmingPolicy::MANUAL,
            base::SequencedTaskRunner::GetCurrentDefault())),
        callback_(std::move(callback)) {
    pipe_watcher_->Watch(
        producer_handle_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
        MOJO_WATCH_CONDITION_SATISFIED,
        base::BindRepeating(&FileSystemReaderDataPipeProducer::OnHandleReady,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  FileSystemReaderDataPipeProducer(const FileSystemReaderDataPipeProducer&) =
      delete;
  FileSystemReaderDataPipeProducer& operator=(
      const FileSystemReaderDataPipeProducer&) = delete;

  void Write() {
    while (remaining_bytes_ > 0) {
      if (!producer_handle_.is_valid())
        CompleteWithResult(net::ERR_FAILED);
      base::span<uint8_t> pipe_buffer;
      MojoResult result = producer_handle_->BeginWriteData(
          kDefaultPipeSize, MOJO_BEGIN_WRITE_DATA_FLAG_NONE, pipe_buffer);
      // If we can't synchronously get the buffer to write to, stop for now and
      // wait for the SimpleWatcher to notify us that the pipe is writable.
      if (result == MOJO_RESULT_SHOULD_WAIT) {
        pipe_watcher_->ArmOrNotify();
        return;
      }
      if (result != MOJO_RESULT_OK) {
        CompleteWithResult(MojoResultToErrorCode(result));
        return;
      }

      DCHECK(base::IsValueInRangeForNumericType<int>(pipe_buffer.size()));
      scoped_refptr<MojoPipeIOBuffer> io_buffer =
          base::MakeRefCounted<MojoPipeIOBuffer>(pipe_buffer.data(),
                                                 pipe_buffer.size());
      const int read_size = stream_reader_->Read(
          io_buffer.get(),
          std::min<int64_t>(pipe_buffer.size(), remaining_bytes_),
          base::BindOnce(
              &FileSystemReaderDataPipeProducer::OnPendingReadComplete,
              weak_ptr_factory_.GetWeakPtr()));
      // Read will return ERR_IO_PENDING if the read couldn't be completed
      // synchronously. In that case return, and OnPendingReadComplete will
      // be called when the read is complete.
      if (read_size == net::ERR_IO_PENDING)
        return;
      net::Error write_error = FinishWrite(read_size);
      if (write_error != net::OK) {
        CompleteWithResult(write_error);
        return;
      }
    }
    CompleteWithResult(net::OK);
  }

  int64_t total_bytes_written() { return total_bytes_written_; }

 private:
  net::Error FinishWrite(int read_size) {
    MojoResult result =
        producer_handle_->EndWriteData(std::max<int>(0, read_size));
    if (read_size <= 0)
      return static_cast<net::Error>(read_size);
    if (result != MOJO_RESULT_OK)
      return MojoResultToErrorCode(result);
    remaining_bytes_ -= read_size;
    total_bytes_written_ += read_size;
    return net::OK;
  }

  void OnPendingReadComplete(int read_result) {
    net::Error result = FinishWrite(read_result);
    if (result != net::OK) {
      CompleteWithResult(result);
      return;
    }
    Write();
  }

  void OnHandleReady(MojoResult result, const mojo::HandleSignalsState& state) {
    if (result != MOJO_RESULT_OK) {
      CompleteWithResult(MojoResultToErrorCode(result));
      return;
    }
    Write();
  }

  void CompleteWithResult(net::Error error) {
    pipe_watcher_.reset();
    std::move(callback_).Run(error);
  }

  net::Error MojoResultToErrorCode(MojoResult result) {
    switch (result) {
      case MOJO_RESULT_OK:
        return net::OK;
      case MOJO_RESULT_CANCELLED:
        return net::ERR_ABORTED;
      case MOJO_RESULT_DEADLINE_EXCEEDED:
        return net::ERR_TIMED_OUT;
      case MOJO_RESULT_NOT_FOUND:
        return net::ERR_FILE_NOT_FOUND;
      case MOJO_RESULT_PERMISSION_DENIED:
        return net::ERR_ACCESS_DENIED;
      case MOJO_RESULT_RESOURCE_EXHAUSTED:
        return net::ERR_INSUFFICIENT_RESOURCES;
      case MOJO_RESULT_UNIMPLEMENTED:
        return net::ERR_NOT_IMPLEMENTED;
      default:
        return net::ERR_FAILED;
    }
  }

  mojo::ScopedDataPipeProducerHandle producer_handle_;
  std::unique_ptr<storage::FileStreamReader> stream_reader_;
  int64_t remaining_bytes_;
  int64_t total_bytes_written_;
  std::unique_ptr<mojo::SimpleWatcher> pipe_watcher_;
  base::OnceCallback<void(net::Error)> callback_;
  base::WeakPtrFactory<FileSystemReaderDataPipeProducer> weak_ptr_factory_{
      this};
};

class ExternalFileURLLoader : public network::mojom::URLLoader {
 public:
  static void CreateAndStart(
      void* profile_id,
      const network::ResourceRequest& request,
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client_remote) {
    // Owns itself. Will live as long as its URLLoader and URLLoaderClient
    // bindings are alive - essentially until either the client gives up or all
    // file data has been sent to it.
    auto* external_file_url_loader = new ExternalFileURLLoader(
        profile_id, std::move(loader), std::move(client_remote));
    external_file_url_loader->Start(request);
  }

  ExternalFileURLLoader(const ExternalFileURLLoader&) = delete;
  ExternalFileURLLoader& operator=(const ExternalFileURLLoader&) = delete;

  // network::mojom::URLLoader:
  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers,
      const std::optional<GURL>& new_url) override {}
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override {}
  void PauseReadingBodyFromNet() override {}
  void ResumeReadingBodyFromNet() override {}

 private:
  explicit ExternalFileURLLoader(
      void* profile_id,
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client_remote)
      : resolver_(std::make_unique<ExternalFileResolver>(profile_id)) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    receiver_.Bind(std::move(loader));
    receiver_.set_disconnect_handler(base::BindOnce(
        &ExternalFileURLLoader::OnMojoDisconnect, base::Unretained(this)));
    client_.Bind(std::move(client_remote));
  }
  ~ExternalFileURLLoader() override = default;

  void Start(const network::ResourceRequest& request) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    head_.request_start = base::TimeTicks::Now();

    resolver_->ProcessHeaders(request.headers);
    resolver_->Resolve(
        request.method, request.url,
        base::BindOnce(&ExternalFileURLLoader::CompleteWithError,
                       weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&ExternalFileURLLoader::OnRedirectURLObtained,
                       weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&ExternalFileURLLoader::OnStreamObtained,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void OnRedirectURLObtained(const std::string& mime_type,
                             const GURL& redirect_url) {
    head_.mime_type = mime_type;
    head_.response_start = base::TimeTicks::Now();
    head_.encoded_data_length = 0;
    net::RedirectInfo redirect_info;
    redirect_info.new_method = "GET";
    redirect_info.status_code = 302;
    redirect_info.new_url = redirect_url;
    client_->OnReceiveRedirect(redirect_info, head_.Clone());
    client_.reset();
    MaybeDeleteSelf();
  }

  void OnStreamObtained(
      const std::string& mime_type,
      storage::IsolatedContext::ScopedFSHandle isolated_file_system_scope,
      std::unique_ptr<storage::FileStreamReader> stream_reader,
      int64_t size) {
    head_.mime_type = mime_type;
    head_.content_length = size;
    isolated_file_system_scope_ = std::move(isolated_file_system_scope);

    mojo::ScopedDataPipeProducerHandle producer_handle;
    mojo::ScopedDataPipeConsumerHandle consumer_handle;
    if (mojo::CreateDataPipe(kDefaultPipeSize, producer_handle,
                             consumer_handle) != MOJO_RESULT_OK) {
      CompleteWithError(net::ERR_FAILED);
      return;
    }
    head_.response_start = base::TimeTicks::Now();
    client_->OnReceiveResponse(head_.Clone(), std::move(consumer_handle),
                               std::nullopt);

    data_producer_ = std::make_unique<FileSystemReaderDataPipeProducer>(
        std::move(producer_handle), std::move(stream_reader), size,
        base::BindOnce(&ExternalFileURLLoader::OnFileWritten,
                       weak_ptr_factory_.GetWeakPtr()));
    data_producer_->Write();
  }

  void OnFileWritten(net::Error error) {
    int64_t total_bytes_written = data_producer_->total_bytes_written();
    data_producer_.reset();
    if (error != net::OK) {
      CompleteWithError(error);
      return;
    }
    network::URLLoaderCompletionStatus status(net::OK);
    status.encoded_data_length = total_bytes_written;
    status.encoded_body_length = total_bytes_written;
    status.decoded_body_length = total_bytes_written;
    client_->OnComplete(status);
    client_.reset();
    MaybeDeleteSelf();
  }

  void CompleteWithError(net::Error net_error) {
    client_->OnComplete(network::URLLoaderCompletionStatus(net_error));
    client_.reset();
    MaybeDeleteSelf();
  }

  void OnMojoDisconnect() {
    data_producer_.reset();
    client_.reset();
    receiver_.reset();
    MaybeDeleteSelf();
  }

  void MaybeDeleteSelf() {
    if (!receiver_.is_bound() && !client_.is_bound())
      delete this;
  }

  mojo::Receiver<network::mojom::URLLoader> receiver_{this};
  mojo::Remote<network::mojom::URLLoaderClient> client_;

  std::unique_ptr<ExternalFileResolver> resolver_;
  network::mojom::URLResponseHead head_;
  storage::IsolatedContext::ScopedFSHandle isolated_file_system_scope_;
  std::unique_ptr<FileSystemReaderDataPipeProducer> data_producer_;

  base::WeakPtrFactory<ExternalFileURLLoader> weak_ptr_factory_{this};
};

}  // namespace

ExternalFileURLLoaderFactory::ExternalFileURLLoaderFactory(
    void* profile_id,
    int render_process_host_id,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory_receiver)
    : network::SelfDeletingURLLoaderFactory(std::move(factory_receiver)),
      profile_id_(profile_id),
      render_process_host_id_(render_process_host_id) {}

ExternalFileURLLoaderFactory::~ExternalFileURLLoaderFactory() = default;

void ExternalFileURLLoaderFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> loader,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  if (render_process_host_id_ != content::ChildProcessHost::kInvalidUniqueID &&
      !content::ChildProcessSecurityPolicy::GetInstance()->CanRequestURL(
          render_process_host_id_, request.url)) {
    DVLOG(1) << "Denied unauthorized request for "
             << request.url.possibly_invalid_spec();
    ReportBadMessage("Unauthorized externalfile request");
    return;
  }
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&ExternalFileURLLoader::CreateAndStart, profile_id_,
                     request, std::move(loader), std::move(client)));
}

// static
mojo::PendingRemote<network::mojom::URLLoaderFactory>
ExternalFileURLLoaderFactory::Create(void* profile_id,
                                     int render_process_host_id) {
  mojo::PendingRemote<network::mojom::URLLoaderFactory> pending_remote;

  // The ExternalFileURLLoaderFactory will delete itself when there are no more
  // receivers - see the network::SelfDeletingURLLoaderFactory::OnDisconnect
  // method.
  new ExternalFileURLLoaderFactory(
      profile_id, render_process_host_id,
      pending_remote.InitWithNewPipeAndPassReceiver());

  return pending_remote;
}

}  // namespace ash
