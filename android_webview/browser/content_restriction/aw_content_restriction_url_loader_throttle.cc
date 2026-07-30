// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/content_restriction/aw_content_restriction_url_loader_throttle.h"

#include <unistd.h>

#include <algorithm>
#include <limits>

#include "android_webview/browser/content_restriction/aw_content_restriction_blocked_navigation_tracker.h"
#include "android_webview/browser/content_restriction/aw_content_restriction_manager_client.h"
#include "base/base_paths.h"
#include "base/check.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/base/net_errors.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/data_element.h"
#include "services/network/public/mojom/chunked_data_pipe_getter.mojom.h"
#include "services/network/public/mojom/data_pipe_getter.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace android_webview {
namespace {

void WriteDataElementBytesToPipe(
    scoped_refptr<base::RefCountedData<base::ScopedFD>> write_fd_wrapper,
    const network::DataElementBytes* bytes,
    scoped_refptr<network::ResourceRequestBody> request_body,
    scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner) {
  DCHECK(sequenced_task_runner->RunsTasksInCurrentSequence());
  DCHECK(write_fd_wrapper);
  DCHECK(bytes);
  if (!base::WriteFileDescriptor(write_fd_wrapper->data.get(),
                                 bytes->AsStringView())) {
    LOG(ERROR) << "Failed to write data element bytes to pipe";
  }
}

void WriteDataElementFileToPipe(
    scoped_refptr<base::RefCountedData<base::ScopedFD>> write_fd_wrapper,
    const network::DataElementFile* file_element,
    scoped_refptr<network::ResourceRequestBody> request_body,
    scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner) {
  DCHECK(sequenced_task_runner->RunsTasksInCurrentSequence());
  DCHECK(write_fd_wrapper);
  DCHECK(file_element);
  base::File file(file_element->path(),
                  base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    LOG(ERROR) << "Failed to open file for streaming request body: "
               << file_element->path().value();
    return;
  }

  uint64_t offset = file_element->offset();
  uint64_t length = file_element->length();
  if (length == std::numeric_limits<uint64_t>::max()) {
    // Stream the entire remainder of the file.
    int64_t file_len = file.GetLength();
    if (file_len < 0 || offset > static_cast<uint64_t>(file_len)) {
      return;
    }
    length = static_cast<uint64_t>(file_len) - offset;
  }

  if (file.Seek(base::File::FROM_BEGIN, static_cast<int64_t>(offset)) < 0) {
    return;
  }

  // Stream the file contents in chunks to minimize memory footprint.
  uint8_t buffer[4096];
  uint64_t total_read = 0;
  while (total_read < length) {
    size_t to_read =
        std::min(sizeof(buffer), static_cast<size_t>(length - total_read));
    std::optional<size_t> read_result =
        file.ReadAtCurrentPos(base::span(buffer).first(to_read));
    if (!read_result.has_value() || read_result.value() == 0) {
      return;
    }
    if (!base::WriteFileDescriptor(
            write_fd_wrapper->data.get(),
            std::string_view(reinterpret_cast<const char*>(buffer),
                             read_result.value()))) {
      return;
    }
    total_read += read_result.value();
  }
}

// Intermediate component responsible for writing data to the spool file and
// pumping data to the target pipe that represents the actual request payload to
// the network service. The target pipe is supposed to receive the entire
// request payload even if the classification result is received before the
// entire payload is parsed from the source.
class SpoolCoordinator
    : public base::RefCountedDeleteOnSequence<SpoolCoordinator> {
 public:
  explicit SpoolCoordinator(
      scoped_refptr<base::SequencedTaskRunner> task_runner)
      : base::RefCountedDeleteOnSequence<SpoolCoordinator>(task_runner) {
    DETACH_FROM_SEQUENCE(sequence_checker_);
    task_runner->PostTask(FROM_HERE,
                          base::BindOnce(&SpoolCoordinator::InitializeSpoolFile,
                                         base::Unretained(this)));
  }

  void InitializeSpoolFile() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    base::FilePath cache_dir;
    base::FilePath temp_path;
    if (!base::PathService::Get(base::DIR_CACHE, &cache_dir) ||
        !base::CreateTemporaryFileInDir(cache_dir, &temp_path)) {
      LOG(ERROR) << "Failed to create a temporary spool file in App Cache.";
      return;
    }
    spool_file_ = base::File(temp_path, base::File::FLAG_OPEN_ALWAYS |
                                            base::File::FLAG_READ |
                                            base::File::FLAG_WRITE |
                                            base::File::FLAG_DELETE_ON_CLOSE);
    if (!spool_file_.IsValid()) {
      LOG(ERROR) << "Failed to open spool file: "
                 << base::File::ErrorToString(spool_file_.error_details());
    }
  }

  void OnDataAvailable(std::vector<uint8_t> data) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!spool_file_.IsValid()) {
      LOG(ERROR)
          << "Cannot write data to the spool file because it is invalid.";
      return;
    }

    if (!spool_file_.WriteAndCheck(write_offset_, data)) {
      LOG(ERROR)
          << "Failed to reliably write available data to the spool file.";
      return;
    }
    write_offset_ += data.size();

    // If the target pipe is active and waiting for data, pump immediately.
    PumpTargetPipe();
  }

  void OnDataComplete() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    source_data_complete_ = true;
    PumpTargetPipe();
  }

  void StartReading(mojo::ScopedDataPipeProducerHandle target_pipe) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    target_pipe_ = std::move(target_pipe);
    watcher_ = std::make_unique<mojo::SimpleWatcher>(
        FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL,
        base::SequencedTaskRunner::GetCurrentDefault());
    watcher_->Watch(target_pipe_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
                    MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                    base::BindRepeating(&SpoolCoordinator::OnTargetPipeWritable,
                                        base::Unretained(this)));
    PumpTargetPipe();
  }

  void CleanUp() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    watcher_.reset();
    target_pipe_.reset();
    spool_file_.Close();
  }

 private:
  friend class base::RefCountedDeleteOnSequence<SpoolCoordinator>;
  friend class base::DeleteHelper<SpoolCoordinator>;

  ~SpoolCoordinator() = default;

  void PumpTargetPipe() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (target_pipe_.is_valid() && watcher_) {
      watcher_->ArmOrNotify();
    }
  }

  void OnTargetPipeWritable(MojoResult result,
                            const mojo::HandleSignalsState& state) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (result != MOJO_RESULT_OK || !target_pipe_.is_valid() ||
        !spool_file_.IsValid()) {
      CleanUp();
      return;
    }

    // Pump all available data to the target pipe.
    while (read_offset_ < write_offset_) {
      DCHECK_LE(read_offset_, write_offset_);
      uint64_t available_bytes =
          base::checked_cast<uint64_t>(write_offset_ - read_offset_);
      base::span<uint8_t> pipe_buffer;
      MojoResult begin_result = target_pipe_->BeginWriteData(
          base::saturated_cast<size_t>(available_bytes),
          MOJO_BEGIN_WRITE_DATA_FLAG_NONE, pipe_buffer);
      if (begin_result == MOJO_RESULT_SHOULD_WAIT) {
        // Target pipe buffer is full. Wait for the pipe to be writable.
        watcher_->ArmOrNotify();
        return;
      }
      if (begin_result != MOJO_RESULT_OK) {
        LOG(ERROR)
            << "Unable to set up the target pipe for writing spool data: "
            << begin_result;
        CleanUp();
        return;
      }

      size_t bytes_to_read = std::min(
          pipe_buffer.size(), base::saturated_cast<size_t>(available_bytes));
      std::optional<size_t> bytes_read =
          spool_file_.Read(read_offset_, pipe_buffer.first(bytes_to_read));
      if (!bytes_read.has_value() || bytes_read.value() == 0) {
        LOG(ERROR) << "Failed to read from spool file at offset "
                   << read_offset_;
        target_pipe_->EndWriteData(0);
        CleanUp();
        return;
      }

      target_pipe_->EndWriteData(bytes_read.value());
      read_offset_ += bytes_read.value();
    }

    if (read_offset_ == write_offset_ && source_data_complete_) {
      // All spooled data has been fully pumped to the target pipe.
      CleanUp();
    }
  }

  base::File spool_file_;
  mojo::ScopedDataPipeProducerHandle target_pipe_;
  std::unique_ptr<mojo::SimpleWatcher> watcher_;

  SEQUENCE_CHECKER(sequence_checker_);
  bool source_data_complete_ = false;
  int64_t write_offset_ = 0;
  int64_t read_offset_ = 0;
};

}  // namespace

class AwContentRestrictionURLLoaderThrottle::DataPipeStreamerBase
    : public mojo::DataPipeDrainer::Client {
 public:
  ~DataPipeStreamerBase() override = default;

 protected:
  DataPipeStreamerBase(
      scoped_refptr<base::RefCountedData<base::ScopedFD>> write_fd_wrapper,
      scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner)
      : write_fd_wrapper_(std::move(write_fd_wrapper)),
        sequenced_task_runner_(std::move(sequenced_task_runner)) {}

  void OnDataComplete() override { CleanUp(); }

  virtual void CleanUp() {
    write_fd_wrapper_.reset();
    sequenced_task_runner_.reset();
    drainer_.reset();
  }

  void WriteDataToPipe(std::vector<uint8_t> data) {
    DCHECK(sequenced_task_runner_);
    DCHECK(write_fd_wrapper_);
    sequenced_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](scoped_refptr<base::RefCountedData<base::ScopedFD>> fd_wrapper,
               std::vector<uint8_t> data) {
              DCHECK(fd_wrapper);
              if (!base::WriteFileDescriptor(fd_wrapper->data.get(),
                                             base::as_string_view(data))) {
                LOG(ERROR) << "Failed to stream data to the file descriptor";
              }
            },
            write_fd_wrapper_, std::move(data)));
  }

  scoped_refptr<base::RefCountedData<base::ScopedFD>> write_fd_wrapper_;
  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;
  std::unique_ptr<mojo::DataPipeDrainer> drainer_;
};

class AwContentRestrictionURLLoaderThrottle::NonChunkedDataPipeStreamer
    : public DataPipeStreamerBase {
 public:
  static std::unique_ptr<NonChunkedDataPipeStreamer> Create(
      scoped_refptr<base::RefCountedData<base::ScopedFD>> write_fd_wrapper,
      mojo::PendingRemote<network::mojom::DataPipeGetter> data_pipe_getter,
      scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner) {
    mojo::ScopedDataPipeProducerHandle producer;
    mojo::ScopedDataPipeConsumerHandle consumer;
    if (mojo::CreateDataPipe(/*options=*/nullptr, producer, consumer) !=
        MOJO_RESULT_OK) {
      LOG(ERROR) << "Failed to create data pipe";
      return nullptr;
    }

    std::unique_ptr<NonChunkedDataPipeStreamer> streamer =
        base::WrapUnique(new NonChunkedDataPipeStreamer(
            std::move(write_fd_wrapper), std::move(consumer),
            std::move(sequenced_task_runner)));
    streamer->data_pipe_getter_.Bind(std::move(data_pipe_getter));
    streamer->data_pipe_getter_.set_disconnect_handler(
        base::BindOnce(&NonChunkedDataPipeStreamer::CleanUp,
                       streamer->weak_factory_.GetWeakPtr()));
    streamer->data_pipe_getter_->Read(
        std::move(producer),
        base::BindOnce(&NonChunkedDataPipeStreamer::OnReadComplete,
                       streamer->weak_factory_.GetWeakPtr()));
    return streamer;
  }

  ~NonChunkedDataPipeStreamer() override { CleanUp(); }

 private:
  NonChunkedDataPipeStreamer(
      scoped_refptr<base::RefCountedData<base::ScopedFD>> write_fd_wrapper,
      mojo::ScopedDataPipeConsumerHandle consumer_handle,
      scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner)
      : DataPipeStreamerBase(std::move(write_fd_wrapper),
                             std::move(sequenced_task_runner)) {
    drainer_ = std::make_unique<mojo::DataPipeDrainer>(
        this, std::move(consumer_handle));
  }

  // mojo::DataPipeDrainer::Client:
  void OnDataAvailable(base::span<const uint8_t> data) override {
    WriteDataToPipe(std::vector<uint8_t>(data.begin(), data.end()));
  }

  void OnReadComplete(int32_t status, uint64_t size) {
    if (status != net::OK) {
      CleanUp();
    }
  }

  mojo::Remote<network::mojom::DataPipeGetter> data_pipe_getter_;
  base::WeakPtrFactory<NonChunkedDataPipeStreamer> weak_factory_{this};
};

class AwContentRestrictionURLLoaderThrottle::ChunkedDataPipeStreamer
    : public DataPipeStreamerBase {
 public:
  static std::unique_ptr<ChunkedDataPipeStreamer> Create(
      scoped_refptr<base::RefCountedData<base::ScopedFD>> write_fd_wrapper,
      scoped_refptr<SpoolCoordinator> spool_coordinator,
      mojo::ScopedDataPipeConsumerHandle source_consumer,
      scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner) {
    return base::WrapUnique(new ChunkedDataPipeStreamer(
        std::move(write_fd_wrapper), std::move(spool_coordinator),
        std::move(source_consumer), std::move(sequenced_task_runner)));
  }

  ~ChunkedDataPipeStreamer() override { CleanUp(); }

  void OnDataAvailable(base::span<const uint8_t> data) override {
    std::vector<uint8_t> data_vector(data.begin(), data.end());
    WriteDataToPipe(data_vector);

    // Spool incoming payload in parallel through the spool coordinator.
    // Spooling is leveraged to account for backpressure considering there are
    // no consumers until we have a verdict on the request classification.
    DCHECK(spool_coordinator_);
    DCHECK(sequenced_task_runner_);
    sequenced_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&SpoolCoordinator::OnDataAvailable,
                                  spool_coordinator_, std::move(data_vector)));
  }

  void OnDataComplete() override {
    DCHECK(spool_coordinator_);
    DCHECK(sequenced_task_runner_);
    sequenced_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&SpoolCoordinator::OnDataComplete, spool_coordinator_));
    DataPipeStreamerBase::OnDataComplete();
  }

 private:
  ChunkedDataPipeStreamer(
      scoped_refptr<base::RefCountedData<base::ScopedFD>> write_fd_wrapper,
      scoped_refptr<SpoolCoordinator> spool_coordinator,
      mojo::ScopedDataPipeConsumerHandle source_consumer,
      scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner)
      : DataPipeStreamerBase(std::move(write_fd_wrapper),
                             std::move(sequenced_task_runner)),
        spool_coordinator_(std::move(spool_coordinator)) {
    drainer_ = std::make_unique<mojo::DataPipeDrainer>(
        this, std::move(source_consumer));
  }

  void CleanUp() override {
    DataPipeStreamerBase::CleanUp();
    spool_coordinator_.reset();
  }

  scoped_refptr<SpoolCoordinator> spool_coordinator_;
};

class AwContentRestrictionURLLoaderThrottle::ProxyChunkedDataPipeGetter
    : public network::mojom::ChunkedDataPipeGetter {
 public:
  ProxyChunkedDataPipeGetter(
      scoped_refptr<SpoolCoordinator> spool_coordinator,
      mojo::PendingRemote<network::mojom::ChunkedDataPipeGetter>
          original_remote,
      scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner)
      : spool_coordinator_(std::move(spool_coordinator)),
        original_remote_(std::move(original_remote)),
        sequenced_task_runner_(std::move(sequenced_task_runner)),
        receiver_(this) {}

  mojo::PendingRemote<network::mojom::ChunkedDataPipeGetter> Bind() {
    mojo::PendingRemote<network::mojom::ChunkedDataPipeGetter> remote;
    receiver_.Bind(remote.InitWithNewPipeAndPassReceiver());
    return remote;
  }

  void GetSize(GetSizeCallback callback) override {
    if (original_remote_) {
      original_remote_->GetSize(std::move(callback));
    } else {
      std::move(callback).Run(net::OK, 0);
    }
  }

  void StartReading(mojo::ScopedDataPipeProducerHandle pipe) override {
    DCHECK(sequenced_task_runner_);
    DCHECK(spool_coordinator_);
    sequenced_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&SpoolCoordinator::StartReading,
                                  spool_coordinator_, std::move(pipe)));
  }

 private:
  scoped_refptr<SpoolCoordinator> spool_coordinator_;
  mojo::Remote<network::mojom::ChunkedDataPipeGetter> original_remote_;
  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;
  mojo::Receiver<network::mojom::ChunkedDataPipeGetter> receiver_;
};

void AwContentRestrictionURLLoaderThrottle::WriteRequestBodyToPipe(
    int write_fd,
    scoped_refptr<network::ResourceRequestBody> request_body) {
  scoped_refptr<base::RefCountedData<base::ScopedFD>> write_fd_wrapper =
      base::MakeRefCounted<base::RefCountedData<base::ScopedFD>>(
          base::ScopedFD(write_fd));
  CHECK(request_body);

  std::vector<network::DataElement>* elements =
      request_body->elements_mutable();
  DCHECK(elements);
  for (network::DataElement& element : *elements) {
    switch (element.type()) {
      case network::DataElement::Tag::kBytes: {
        // Pass the `request_body` to guarantee safe memory access
        // until the request body is fully written to the pipe.
        sequenced_task_runner_->PostTask(
            FROM_HERE,
            base::BindOnce(&WriteDataElementBytesToPipe, write_fd_wrapper,
                           &element.As<network::DataElementBytes>(),
                           request_body, sequenced_task_runner_));
        break;
      }
      case network::DataElement::Tag::kFile: {
        // Pass the `request_body` to guarantee safe memory access
        // until file contents are fully written to the pipe.
        sequenced_task_runner_->PostTask(
            FROM_HERE,
            base::BindOnce(&WriteDataElementFileToPipe, write_fd_wrapper,
                           &element.As<network::DataElementFile>(),
                           request_body, sequenced_task_runner_));
        break;
      }
      case network::DataElement::Tag::kDataPipe: {
        std::unique_ptr<NonChunkedDataPipeStreamer> streamer =
            NonChunkedDataPipeStreamer::Create(
                write_fd_wrapper,
                element.As<network::DataElementDataPipe>()
                    .CloneDataPipeGetter(),
                sequenced_task_runner_);
        if (streamer) {
          streamers_.push_back(std::move(streamer));
        }
        break;
      }
      case network::DataElement::Tag::kChunkedDataPipe: {
        // Because we cannot clone a ChunkedDataPipeGetter, we will need to
        // regenerate the request payload post classification. We will spool
        // data as we receive them so it can be subsequently used by the proxy
        // pipe getter that replaces the original one.
        WriteChunkedDataPipeToPipe(write_fd_wrapper, element);
        break;
      }
      default: {
        NOTREACHED();
      }
    }
  }
}

void AwContentRestrictionURLLoaderThrottle::WriteChunkedDataPipeToPipe(
    scoped_refptr<base::RefCountedData<base::ScopedFD>> write_fd_wrapper,
    network::DataElement& element) {
  scoped_refptr<SpoolCoordinator> spool_coordinator =
      base::MakeRefCounted<SpoolCoordinator>(sequenced_task_runner_);

  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  MojoResult result = mojo::CreateDataPipe(nullptr, producer, consumer);
  if (result != MOJO_RESULT_OK) {
    LOG(ERROR) << "Failed to create data pipe: " << result;
    return;
  }

  network::DataElementChunkedDataPipe& chunked_element =
      element.As<network::DataElementChunkedDataPipe>();
  mojo::Remote<network::mojom::ChunkedDataPipeGetter> original_remote(
      chunked_element.ReleaseChunkedDataPipeGetter());
  original_remote->StartReading(std::move(producer));

  std::unique_ptr<ProxyChunkedDataPipeGetter> proxy_getter =
      std::make_unique<ProxyChunkedDataPipeGetter>(
          spool_coordinator, original_remote.Unbind(), sequenced_task_runner_);
  std::unique_ptr<ChunkedDataPipeStreamer> proxy_streamer =
      ChunkedDataPipeStreamer::Create(write_fd_wrapper, spool_coordinator,
                                      std::move(consumer),
                                      sequenced_task_runner_);
  element = network::DataElement(network::DataElementChunkedDataPipe(
      proxy_getter->Bind(),
      network::DataElementChunkedDataPipe::ReadOnlyOnce(true)));
  proxy_chunked_data_pipe_getters_.push_back(std::move(proxy_getter));
  streamers_.push_back(std::move(proxy_streamer));
}

AwContentRestrictionURLLoaderThrottle::AwContentRestrictionURLLoaderThrottle(
    AwContentRestrictionManagerClient* client,
    AwContentRestrictionBlockedNavigationTracker* tracker,
    std::optional<int64_t> navigation_id)
    : content_restriction_manager_client_(client),
      tracker_(tracker),
      navigation_id_(navigation_id),
      sequenced_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT})) {}

AwContentRestrictionURLLoaderThrottle::
    ~AwContentRestrictionURLLoaderThrottle() = default;

void AwContentRestrictionURLLoaderThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
  DCHECK(content_restriction_manager_client_);
  if (navigation_id_.has_value() &&
      content_restriction_manager_client_->IsContentRestrictionEnabled()) {
    *defer = true;

    const int64_t navigation_id = navigation_id_.value();
    if (request->request_body) {
      int write_fd = content_restriction_manager_client_
                         ->CreateRequestBodyPipeAndGetWriteFd(navigation_id);
      if (write_fd >= 0) {
        WriteRequestBodyToPipe(write_fd, request->request_body);
      }
    }

    // Kick off request classification after writing into the pipe. This
    // will offer filtering apps additional time for processing with the payload
    // data that has been already written into the pipe.
    content_restriction_manager_client_->RequestContentClassification(
        navigation_id, *request,
        base::BindOnce(
            &AwContentRestrictionURLLoaderThrottle::OnClassificationResult,
            weak_ptr_factory_.GetWeakPtr()));
  }
}

void AwContentRestrictionURLLoaderThrottle::WillRedirectRequest(
    net::RedirectInfo* redirect_info,
    const network::mojom::URLResponseHead& response_head,
    bool* defer,
    network::HttpRequestHeadersUpdateParams* headers_update_params) {
  DCHECK(content_restriction_manager_client_);
  if (navigation_id_.has_value() &&
      content_restriction_manager_client_->IsContentRestrictionEnabled()) {
    *defer = true;

    // There is no need to share the request body or the content type header
    // value as they will be identical to the original request.
    network::ResourceRequest request;
    request.url = redirect_info->new_url;
    request.method = redirect_info->new_method;
    content_restriction_manager_client_->RequestContentClassification(
        navigation_id_.value(), request,
        base::BindOnce(
            &AwContentRestrictionURLLoaderThrottle::OnClassificationResult,
            weak_ptr_factory_.GetWeakPtr()));
  }
}

void AwContentRestrictionURLLoaderThrottle::OnClassificationResult(
    bool is_allowed) {
  DCHECK(delegate_);
  DCHECK(tracker_);
  if (is_allowed) {
    delegate_->Resume();
    return;
  }

  if (navigation_id_.has_value()) {
    tracker_->RegisterNavigationAsBlocked(navigation_id_.value());
  }
  delegate_->CancelWithError(net::ERR_BLOCKED_BY_CLIENT);
}

}  // namespace android_webview
