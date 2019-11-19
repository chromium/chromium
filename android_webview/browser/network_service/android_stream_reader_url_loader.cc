// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/network_service/android_stream_reader_url_loader.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "android_webview/browser/input_stream.h"
#include "android_webview/browser/network_service/input_stream_reader.h"
#include "android_webview/common/aw_features.h"
#include "base/android/jni_android.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/io_buffer.h"
#include "net/base/mime_sniffer.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/url_loader_completion_status.h"

namespace android_webview {

namespace {

const char kResponseHeaderViaShouldInterceptRequest[] =
    "Client-Via: shouldInterceptRequest";
const char kHTTPOkText[] = "OK";
const char kHTTPNotFoundText[] = "Not Found";

}  // namespace

namespace {

using OnInputStreamOpenedCallback = base::OnceCallback<void(
    std::unique_ptr<AndroidStreamReaderURLLoader::ResponseDelegate>,
    std::unique_ptr<InputStream>)>;

// static
void OpenInputStreamOnWorkerThread(
    scoped_refptr<base::SingleThreadTaskRunner> job_thread_task_runner,
    std::unique_ptr<AndroidStreamReaderURLLoader::ResponseDelegate> delegate,
    OnInputStreamOpenedCallback callback) {
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  JNIEnv* env = base::android::AttachCurrentThread();
  DCHECK(env);

  std::unique_ptr<InputStream> input_stream = delegate->OpenInputStream(env);

  job_thread_task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(delegate),
                                std::move(input_stream)));
}

}  // namespace

// In the case when stream reader related tasks are posted on a dedicated
// thread they can outlive the loader. This is a wrapper is for holding both
// InputStream and InputStreamReader to ensure they are still there when the
// task is run.
class InputStreamReaderWrapper
    : public base::RefCountedThreadSafe<InputStreamReaderWrapper> {
 public:
  InputStreamReaderWrapper(
      std::unique_ptr<InputStream> input_stream,
      std::unique_ptr<InputStreamReader> input_stream_reader)
      : input_stream_(std::move(input_stream)),
        input_stream_reader_(std::move(input_stream_reader)) {
    DCHECK(input_stream_);
    DCHECK(input_stream_reader_);
  }

  InputStream* input_stream() { return input_stream_.get(); }

  int Seek(const net::HttpByteRange& byte_range) {
    return input_stream_reader_->Seek(byte_range);
  }

  int ReadRawData(net::IOBuffer* buffer, int buffer_size) {
    return input_stream_reader_->ReadRawData(buffer, buffer_size);
  }

 private:
  friend class base::RefCountedThreadSafe<InputStreamReaderWrapper>;
  ~InputStreamReaderWrapper() {}

  std::unique_ptr<InputStream> input_stream_;
  std::unique_ptr<InputStreamReader> input_stream_reader_;

  DISALLOW_COPY_AND_ASSIGN(InputStreamReaderWrapper);
};

AndroidStreamReaderURLLoader::AndroidStreamReaderURLLoader(
    const network::ResourceRequest& resource_request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    std::unique_ptr<ResponseDelegate> response_delegate)
    : resource_request_(resource_request),
      response_head_(network::mojom::URLResponseHead::New()),
      client_(std::move(client)),
      traffic_annotation_(traffic_annotation),
      response_delegate_(std::move(response_delegate)),
      writable_handle_watcher_(FROM_HERE,
                               mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                               base::SequencedTaskRunnerHandle::Get()) {
  DCHECK(response_delegate_);
  // If there is a client error, clean up the request.
  client_.set_disconnect_handler(
      base::BindOnce(&AndroidStreamReaderURLLoader::RequestComplete,
                     weak_factory_.GetWeakPtr(), net::ERR_ABORTED));
}

AndroidStreamReaderURLLoader::~AndroidStreamReaderURLLoader() {}

void AndroidStreamReaderURLLoader::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const base::Optional<GURL>& new_url) {}
void AndroidStreamReaderURLLoader::SetPriority(net::RequestPriority priority,
                                               int intra_priority_value) {}
void AndroidStreamReaderURLLoader::PauseReadingBodyFromNet() {}
void AndroidStreamReaderURLLoader::ResumeReadingBodyFromNet() {}

void AndroidStreamReaderURLLoader::Start() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!ParseRange(resource_request_.headers)) {
    RequestComplete(net::ERR_REQUEST_RANGE_NOT_SATISFIABLE);
    return;
  }

  base::PostTask(
      FROM_HERE, {base::ThreadPool(), base::MayBlock()},
      base::BindOnce(
          &OpenInputStreamOnWorkerThread, base::ThreadTaskRunnerHandle::Get(),
          // This is intentional - the loader could be deleted while the
          // callback is executing on the background thread. The delegate will
          // be "returned" to the loader once the InputStream open attempt is
          // completed.
          std::move(response_delegate_),
          base::BindOnce(&AndroidStreamReaderURLLoader::OnInputStreamOpened,
                         weak_factory_.GetWeakPtr())));
}

void AndroidStreamReaderURLLoader::OnInputStreamOpened(
    std::unique_ptr<AndroidStreamReaderURLLoader::ResponseDelegate>
        returned_delegate,
    std::unique_ptr<InputStream> input_stream) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(returned_delegate);
  response_delegate_ = std::move(returned_delegate);

  if (!input_stream) {
    bool restarted_or_completed = response_delegate_->OnInputStreamOpenFailed();
    if (restarted_or_completed) {
      // The original request has been restarted with a new loader or
      // completed. We can clean up this loader.
      // Generally speaking this can happen in the following cases
      // (see aw_proxying_url_loader_factory.cc for delegate implementation):
      //   1. InterceptResponseDelegate :
      //     - intercepted requests with custom response,
      //     - no restart required
      //   2. ProtocolResponseDelegate :
      //     - e.g. file:///android_asset/,
      //     - restart required
      //   3. InterceptResponseDelegate, intercept-only setting :
      //     - used for external protocols,
      //     - no restart, but completes immediately.
      CleanUp();
    } else {
      HeadersComplete(net::HTTP_NOT_FOUND, kHTTPNotFoundText);
    }
    return;
  }

  auto input_stream_reader =
      std::make_unique<InputStreamReader>(input_stream.get());
  DCHECK(input_stream);
  DCHECK(!input_stream_reader_wrapper_);

  input_stream_reader_wrapper_ = base::MakeRefCounted<InputStreamReaderWrapper>(
      std::move(input_stream), std::move(input_stream_reader));

  base::PostTaskAndReplyWithResult(
      FROM_HERE, {base::ThreadPool(), base::MayBlock()},
      base::BindOnce(&InputStreamReaderWrapper::Seek,
                     input_stream_reader_wrapper_, byte_range_),
      base::BindOnce(&AndroidStreamReaderURLLoader::OnReaderSeekCompleted,
                     weak_factory_.GetWeakPtr()));
}

void AndroidStreamReaderURLLoader::OnReaderSeekCompleted(int result) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (result >= 0) {
    // we've got the expected content size here
    expected_content_size_ = result;
    HeadersComplete(net::HTTP_OK, kHTTPOkText);
  } else {
    RequestComplete(net::ERR_FAILED);
  }
}

void AndroidStreamReaderURLLoader::HeadersComplete(
    int status_code,
    const std::string& status_text) {
  DCHECK(thread_checker_.CalledOnValidThread());

  std::string status("HTTP/1.1 ");
  status.append(base::NumberToString(status_code));
  status.append(" ");
  status.append(status_text);
  // HttpResponseHeaders expects its input string to be terminated by two NULs.
  status.append("\0\0", 2);

  network::mojom::URLResponseHead& head = *response_head_;
  head.request_start = base::TimeTicks::Now();
  head.response_start = base::TimeTicks::Now();
  head.headers = new net::HttpResponseHeaders(status);

  JNIEnv* env = base::android::AttachCurrentThread();
  DCHECK(env);

  if (status_code == net::HTTP_OK) {
    response_delegate_->GetCharset(env, resource_request_.url,
                                   input_stream_reader_wrapper_->input_stream(),
                                   &head.charset);

    if (expected_content_size_ != -1) {
      std::string content_length_header(
          net::HttpRequestHeaders::kContentLength);
      content_length_header.append(": ");
      content_length_header.append(
          base::NumberToString(expected_content_size_));
      head.headers->AddHeader(content_length_header);
      head.content_length = expected_content_size_;
    }

    std::string mime_type;
    if (response_delegate_->GetMimeType(
            env, resource_request_.url,
            input_stream_reader_wrapper_->input_stream(), &mime_type) &&
        !mime_type.empty()) {
      std::string content_type_header(net::HttpRequestHeaders::kContentType);
      content_type_header.append(": ");
      content_type_header.append(mime_type);
      head.headers->AddHeader(content_type_header);
      head.mime_type = mime_type;
    }
  }

  response_delegate_->AppendResponseHeaders(env, head.headers.get());

  // Indicate that the response had been obtained via shouldInterceptRequest.
  // TODO(jam): why is this added for protocol handler (e.g. content scheme and
  // file resources?). The old path does this as well.
  head.headers->AddHeader(kResponseHeaderViaShouldInterceptRequest);

  SendBody();
}

void AndroidStreamReaderURLLoader::SendBody() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (CreateDataPipe(nullptr /*options*/, &producer_handle_,
                     &consumer_handle_) != MOJO_RESULT_OK) {
    RequestComplete(net::ERR_FAILED);
    return;
  }
  writable_handle_watcher_.Watch(
      producer_handle_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
      base::BindRepeating(&AndroidStreamReaderURLLoader::OnDataPipeWritable,
                          base::Unretained(this)));

  // Send the response if possible now for 2 reasons:
  // 1. If we don't need any more MIME type sniffing, there's no reason not to
  //    tell the URLLoaderClient right away. Sending now should preserve
  //    ordering between app-visible callbacks and the first read of the
  //    InputStream (although we do not generally guarantee the ordering).
  // 2. Sending this now lets us unittest the net::ERR_ABORTED case. The case
  //    needs the ability to break the stream after getting the headers but
  //    before finishing the read.
  if (!base::FeatureList::IsEnabled(features::kWebViewSniffMimeType) ||
      !response_head_->mime_type.empty()) {
    SendResponseToClient();
  }
  ReadMore();
}

void AndroidStreamReaderURLLoader::SendResponseToClient() {
  DCHECK(consumer_handle_.is_valid());
  DCHECK(client_.is_bound());
  client_->OnReceiveResponse(std::move(response_head_));
  client_->OnStartLoadingResponseBody(std::move(consumer_handle_));
}

void AndroidStreamReaderURLLoader::ReadMore() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!pending_buffer_.get());

  uint32_t num_bytes;
  MojoResult mojo_result = network::NetToMojoPendingBuffer::BeginWrite(
      &producer_handle_, &pending_buffer_, &num_bytes);
  if (mojo_result == MOJO_RESULT_SHOULD_WAIT) {
    // The pipe is full. We need to wait for it to have more space.
    writable_handle_watcher_.ArmOrNotify();
    return;
  } else if (mojo_result == MOJO_RESULT_FAILED_PRECONDITION) {
    // The data pipe consumer handle has been closed.
    RequestComplete(net::ERR_ABORTED);
    return;
  } else if (mojo_result != MOJO_RESULT_OK) {
    // The body stream is in a bad state. Bail out.
    RequestComplete(net::ERR_UNEXPECTED);
    return;
  }
  scoped_refptr<net::IOBuffer> buffer(
      new network::NetToMojoIOBuffer(pending_buffer_.get()));

  if (!input_stream_reader_wrapper_.get()) {
    // This will happen if opening the InputStream fails in which case the
    // error is communicated by setting the HTTP response status header rather
    // than failing the request during the header fetch phase.
    DidRead(0);
    return;
  }

  // TODO(timvolodine): consider using a sequenced task runner.
  base::PostTaskAndReplyWithResult(
      FROM_HERE, {base::ThreadPool(), base::MayBlock()},
      base::BindOnce(
          &InputStreamReaderWrapper::ReadRawData, input_stream_reader_wrapper_,
          base::RetainedRef(buffer.get()), base::checked_cast<int>(num_bytes)),
      base::BindOnce(&AndroidStreamReaderURLLoader::DidRead,
                     weak_factory_.GetWeakPtr()));
}

void AndroidStreamReaderURLLoader::DidRead(int result) {
  DCHECK(thread_checker_.CalledOnValidThread());

  DCHECK(pending_buffer_);
  if (result < 0) {
    // error case
    RequestComplete(result);
    return;
  }
  if (result == 0) {
    // eof, read completed
    pending_buffer_->Complete(0);
    RequestComplete(net::OK);
    return;
  }
  if (consumer_handle_.is_valid()) {
    // We only hit this on for the first buffer read, which we expect to be
    // enough to determine the MIME type.
    DCHECK(base::FeatureList::IsEnabled(features::kWebViewSniffMimeType));
    if (response_head_->mime_type.empty()) {
      // Limit sniffing to the first net::kMaxBytesToSniff.
      size_t data_length = result;
      if (data_length > net::kMaxBytesToSniff)
        data_length = net::kMaxBytesToSniff;

      std::string new_type;
      net::SniffMimeType(pending_buffer_->buffer(), data_length,
                         resource_request_.url, std::string(),
                         net::ForceSniffFileUrlsForHtml::kDisabled, &new_type);
      // SniffMimeType() returns false if there is not enough data to
      // determine the mime type. However, even if it returns false, it
      // returns a new type that is probably better than the current one.
      response_head_->mime_type.assign(new_type);
      response_head_->did_mime_sniff = true;
    }

    SendResponseToClient();
  }

  producer_handle_ = pending_buffer_->Complete(result);
  pending_buffer_ = nullptr;

  // TODO(timvolodine): consider using a sequenced task runner.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&AndroidStreamReaderURLLoader::ReadMore,
                                weak_factory_.GetWeakPtr()));
}

void AndroidStreamReaderURLLoader::OnDataPipeWritable(MojoResult result) {
  if (result == MOJO_RESULT_FAILED_PRECONDITION) {
    RequestComplete(net::ERR_ABORTED);
    return;
  }
  DCHECK_EQ(result, MOJO_RESULT_OK) << result;

  ReadMore();
}

void AndroidStreamReaderURLLoader::RequestComplete(int status_code) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (consumer_handle_.is_valid()) {
    // We can hit this before reading any buffers under error conditions.
    SendResponseToClient();
  }

  client_->OnComplete(network::URLLoaderCompletionStatus(status_code));
  CleanUp();
}

void AndroidStreamReaderURLLoader::CleanUp() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Resets the watchers and pipes, so that we will never be called back.
  writable_handle_watcher_.Cancel();
  pending_buffer_ = nullptr;
  producer_handle_.reset();

  // Manages its own lifetime
  delete this;
}

// TODO(timvolodine): consider moving this to the net_helpers.cc
bool AndroidStreamReaderURLLoader::ParseRange(
    const net::HttpRequestHeaders& headers) {
  DCHECK(thread_checker_.CalledOnValidThread());

  std::string range_header;
  if (headers.GetHeader(net::HttpRequestHeaders::kRange, &range_header)) {
    // This loader only cares about the Range header so that we know how many
    // bytes in the stream to skip and how many to read after that.
    std::vector<net::HttpByteRange> ranges;
    if (net::HttpUtil::ParseRangeHeader(range_header, &ranges)) {
      // In case of multi-range request only use the first range.
      // We don't support multirange requests.
      if (ranges.size() == 1)
        byte_range_ = ranges[0];
    } else {
      // This happens if the range header could not be parsed or is invalid.
      return false;
    }
  }
  return true;
}

}  // namespace android_webview
