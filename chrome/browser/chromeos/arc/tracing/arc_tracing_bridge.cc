// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/tracing/arc_tracing_bridge.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/posix/unix_domain_socket.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_config.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/arc_service_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/service_manager_connection.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace arc {

namespace {

// The maximum size used to store one trace event. The ad hoc trace format for
// atrace is 1024 bytes. Here we add additional size as we're using JSON and
// have additional data fields.
constexpr size_t kArcTraceMessageLength = 1024 + 512;

constexpr char kChromeTraceEventLabel[] = "traceEvents";

// The prefix of the categories to be shown on the trace selection UI.
// The space at the end of the string is intentional as the separator between
// the prefix and the real categories.
constexpr char kCategoryPrefix[] = TRACE_DISABLED_BY_DEFAULT("android ");

// Singleton factory for ArcTracingBridge.
class ArcTracingBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcTracingBridge,
          ArcTracingBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcTracingBridgeFactory";

  static ArcTracingBridgeFactory* GetInstance() {
    return base::Singleton<ArcTracingBridgeFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcTracingBridgeFactory>;
  ArcTracingBridgeFactory() = default;
  ~ArcTracingBridgeFactory() override = default;
};

}  // namespace

struct ArcTracingBridge::Category {
  // The name used by Android to trigger tracing.
  std::string name;
  // The full name shown in the tracing UI in chrome://tracing.
  std::string full_name;
};

// static
ArcTracingBridge* ArcTracingBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcTracingBridgeFactory::GetForBrowserContext(context);
}

ArcTracingBridge::ArcTracingBridge(content::BrowserContext* context,
                                   ArcBridgeService* bridge_service)
    : BaseAgent(
          content::ServiceManagerConnection::GetForProcess()->GetConnector(),
          kChromeTraceEventLabel,
          tracing::mojom::TraceDataType::ARRAY,
          false /* supports_explicit_clock_sync */,
          base::kNullProcessId),
      arc_bridge_service_(bridge_service),
      weak_ptr_factory_(this) {
  arc_bridge_service_->tracing()->AddObserver(this);
}

ArcTracingBridge::~ArcTracingBridge() {
  arc_bridge_service_->tracing()->RemoveObserver(this);
}

void ArcTracingBridge::OnConnectionReady() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  mojom::TracingInstance* tracing_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->tracing(), QueryAvailableCategories);
  if (!tracing_instance)
    return;
  tracing_instance->QueryAvailableCategories(base::BindOnce(
      &ArcTracingBridge::OnCategoriesReady, weak_ptr_factory_.GetWeakPtr()));
}

void ArcTracingBridge::OnCategoriesReady(
    const std::vector<std::string>& categories) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // There is no API in TraceLog to remove a category from the UI. As an
  // alternative, the old category that is no longer in |categories_| will be
  // ignored when calling |StartTracing|.
  categories_.clear();
  for (const auto& category : categories) {
    categories_.emplace_back(Category{category, kCategoryPrefix + category});
    // Show the category name in the selection UI.
    base::trace_event::TraceLog::GetCategoryGroupEnabled(
        categories_.back().full_name.c_str());
  }
}

void ArcTracingBridge::StartTracing(const std::string& config,
                                    base::TimeTicks coordinator_time,
                                    Agent::StartTracingCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::trace_event::TraceConfig trace_config(config);

  base::ScopedFD write_fd, read_fd;
  bool success =
      trace_config.IsSystraceEnabled() && CreateSocketPair(&read_fd, &write_fd);

  if (!success) {
    // Use PostTask as the convention of TracingAgent. The caller expects
    // callback to be called after this function returns.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false /* success */));
    return;
  }

  mojom::TracingInstance* tracing_instance =
      ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->tracing(), StartTracing);
  if (!tracing_instance) {
    // Use PostTask as the convention of TracingAgent. The caller expects
    // callback to be called after this function returns.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

  std::vector<std::string> selected_categories;
  for (const auto& category : categories_) {
    if (trace_config.IsCategoryGroupEnabled(category.full_name))
      selected_categories.push_back(category.name);
  }

  tracing_instance->StartTracing(selected_categories,
                                 mojo::WrapPlatformFile(write_fd.release()),
                                 std::move(callback));

  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(&ArcTracingReader::StartTracing, reader_.GetWeakPtr(),
                     std::move(read_fd)));
}

void ArcTracingBridge::StopAndFlush(tracing::mojom::RecorderPtr recorder) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (is_stopping_) {
    DLOG(WARNING) << "Already working on stopping ArcTracingAgent.";
    return;
  }
  is_stopping_ = true;
  recorder_ = std::move(recorder);

  mojom::TracingInstance* tracing_instance =
      ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->tracing(), StopTracing);
  if (!tracing_instance) {
    OnArcTracingStopped(false);
    return;
  }
  tracing_instance->StopTracing(base::BindOnce(
      &ArcTracingBridge::OnArcTracingStopped, weak_ptr_factory_.GetWeakPtr()));
}

void ArcTracingBridge::OnArcTracingStopped(bool success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!success) {
    DLOG(WARNING) << "Failed to stop ARC tracing.";
    is_stopping_ = false;
    return;
  }
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(&ArcTracingReader::StopTracing, reader_.GetWeakPtr(),
                     base::BindOnce(&ArcTracingBridge::OnTracingReaderStopped,
                                    weak_ptr_factory_.GetWeakPtr())));
}

void ArcTracingBridge::OnTracingReaderStopped(const std::string& data) {
  recorder_->AddChunk(data);
  recorder_.reset();
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  is_stopping_ = false;
}

ArcTracingBridge::ArcTracingReader::ArcTracingReader()
    : weak_ptr_factory_(this) {}

ArcTracingBridge::ArcTracingReader::~ArcTracingReader() {
  DCHECK(!fd_watcher_);
}

void ArcTracingBridge::ArcTracingReader::StartTracing(base::ScopedFD read_fd) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  read_fd_ = std::move(read_fd);
  // We don't use the weak pointer returned by |GetWeakPtr| to avoid using it
  // on different task runner. Instead, we use |base::Unretained| here as
  // |fd_watcher_| is always destroyed before |this| is destroyed.
  fd_watcher_ = base::FileDescriptorWatcher::WatchReadable(
      read_fd_.get(),
      base::BindRepeating(&ArcTracingReader::OnTraceDataAvailable,
                          base::Unretained(this)));
}

void ArcTracingBridge::ArcTracingReader::OnTraceDataAvailable() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  char buf[kArcTraceMessageLength];
  std::vector<base::ScopedFD> unused_fds;
  ssize_t n = base::UnixDomainSocket::RecvMsg(
      read_fd_.get(), buf, kArcTraceMessageLength, &unused_fds);
  if (n < 0) {
    DPLOG(WARNING) << "Unexpected error while reading trace from client.";
    // Do nothing here as StopTracing will do the clean up and the existing
    // trace logs will be returned.
    return;
  }

  if (n == 0) {
    // When EOF is reached, stop listening for changes since there's never
    // going to be any more data to be read. The rest of the cleanup will be
    // done in StopTracing.
    fd_watcher_.reset();
    return;
  }

  if (n > static_cast<ssize_t>(kArcTraceMessageLength)) {
    DLOG(WARNING) << "Unexpected data size when reading trace from client.";
    return;
  }
  ring_buffer_.SaveToBuffer(std::string(buf, n));
}

void ArcTracingBridge::ArcTracingReader::StopTracing(
    base::OnceCallback<void(const std::string&)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  fd_watcher_.reset();
  read_fd_.reset();

  bool append_comma = false;
  std::string data;
  for (auto it = ring_buffer_.Begin(); it; ++it) {
    if (append_comma)
      data.append(",");
    else
      append_comma = true;
    data.append(**it);
  }
  ring_buffer_.Clear();

  base::PostTaskWithTraits(FROM_HERE, {content::BrowserThread::UI},
                           base::BindOnce(std::move(callback), data));
}

base::WeakPtr<ArcTracingBridge::ArcTracingReader>
ArcTracingBridge::ArcTracingReader::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace arc
