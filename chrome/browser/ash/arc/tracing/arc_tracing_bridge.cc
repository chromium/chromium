// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/tracing/arc_tracing_bridge.h"

#include <utility>

#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ash/components/arc/mojom/tracing.mojom.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "base/files/file.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "base/no_destructor.h"
#include "base/posix/unix_domain_socket.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_config.h"
#include "base/trace_event/trace_event.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "services/tracing/public/cpp/perfetto/perfetto_traced_process.h"
#include "services/tracing/public/cpp/perfetto/system_trace_writer.h"
#include "services/tracing/public/mojom/constants.mojom.h"
#include "services/tracing/public/mojom/perfetto_service.mojom.h"

namespace arc {

namespace {

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

// Perfetto data source which coordinates ARC tracing sessions with perfetto's
// PerfettoProducer when perfetto is used as the tracing backend.
class ArcTracingDataSource
    : public tracing::PerfettoTracedProcess::DataSourceBase {
 public:
  static ArcTracingDataSource* GetInstance() {
    static base::NoDestructor<ArcTracingDataSource> instance;
    return instance.get();
  }

  ArcTracingDataSource(const ArcTracingDataSource&) = delete;
  ArcTracingDataSource& operator=(const ArcTracingDataSource&) = delete;

  // Called after constructing |bridge|.
  void RegisterBridgeOnUI(ArcTracingBridge* bridge) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    DCHECK_EQ(ArcTracingBridge::State::kDisabled, bridge->state());
    bool success = bridges_.insert(bridge).second;
    DCHECK(success);

    if (producer_on_ui_thread_ && !stop_complete_callback_) {
      // We're currently tracing, so start the new bridge, too.
      // |this| never gets destructed, so it's OK to bind an unretained pointer.
      bridge->StartTracing(
          data_source_config_.chrome_config().trace_config(),
          base::BindOnce(&ArcTracingDataSource::OnTracingStartedOnUI,
                         base::Unretained(this)));
    }
  }

  // Called when destructing |bridge|.
  void UnregisterBridgeOnUI(ArcTracingBridge* bridge) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    const size_t erase_count = bridges_.erase(bridge);
    DCHECK_EQ(1u, erase_count);

    // Make sure we don't continue to wait for any of the bridge's callbacks.
    OnTracingStartedOnUI(false /*success*/);
    OnTracingStoppedOnUI();
  }

 private:
  friend class base::NoDestructor<ArcTracingDataSource>;
  using DataSourceProxy =
      tracing::PerfettoTracedProcess::DataSourceProxy<ArcTracingDataSource>;
  using SystemTraceWriter =
      tracing::SystemTraceWriter<std::string, DataSourceProxy>;

  ArcTracingDataSource()
      : DataSourceBase(tracing::mojom::kArcTraceDataSourceName),
        perfetto_task_runner_(tracing::PerfettoTracedProcess::Get()
                                  ->GetTaskRunner()
                                  ->GetOrCreateTaskRunner()) {
    tracing::PerfettoTracedProcess::Get()->AddDataSource(this);
    perfetto::DataSourceDescriptor dsd;
    dsd.set_name(tracing::mojom::kArcTraceDataSourceName);
    DataSourceProxy::Register(dsd, this);
  }

  // Note that ArcTracingDataSource is a singleton that's never destroyed.
  ~ArcTracingDataSource() override = default;

  // tracing::PerfettoProducer::DataSourceBase.
  void StartTracingImpl(
      tracing::PerfettoProducer* producer,
      const perfetto::DataSourceConfig& data_source_config) override {
    // |this| never gets destructed, so it's OK to bind an unretained pointer.
    // |producer| is a singleton that is never destroyed.
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&ArcTracingDataSource::StartTracingOnUI,
                       base::Unretained(this), producer, data_source_config));
  }

  void StopTracingImpl(base::OnceClosure stop_complete_callback) override {
    // |this| never gets destructed, so it's OK to bind an unretained pointer.
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&ArcTracingDataSource::StopTracingOnUI,
                                  base::Unretained(this),
                                  std::move(stop_complete_callback)));
  }

  void Flush(base::RepeatingClosure flush_complete_callback) override {
    // ARC's tracing service doesn't currently support flushing while recording.
    flush_complete_callback.Run();
  }

  // Starts all registered bridges.
  void StartTracingOnUI(tracing::PerfettoProducer* producer,
                        const perfetto::DataSourceConfig& data_source_config) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    DCHECK(!producer_on_ui_thread_);

    producer_on_ui_thread_ = producer;
    data_source_config_ = data_source_config;

    for (ArcTracingBridge* bridge : bridges_) {
      // |this| never gets destructed, so it's OK to bind an unretained pointer.
      bridge->StartTracing(
          data_source_config_.chrome_config().trace_config(),
          base::BindOnce(&ArcTracingDataSource::OnTracingStartedOnUI,
                         base::Unretained(this)));
    }
  }

  // Stops all registered bridges. Calls |stop_complete_callback| when all
  // bridges have stopped.
  void StopTracingOnUI(base::OnceClosure stop_complete_callback) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    // We may receive a StopTracing without StartTracing.
    if (!producer_on_ui_thread_) {
      perfetto_task_runner_->PostTask(FROM_HERE,
                                      std::move(stop_complete_callback));
      return;
    }

    // We may still be in startup. In this case, store a callback to rerun
    // StopTracingOnUI() once startup is complete.
    if (IsAnyBridgeStarting()) {
      DCHECK(!pending_stop_tracing_);
      pending_stop_tracing_ = base::BindOnce(
          &ArcTracingDataSource::StopTracingOnUI, base::Unretained(this),
          std::move(stop_complete_callback));
      return;
    }

    stop_complete_callback_ = std::move(stop_complete_callback);

    for (ArcTracingBridge* bridge : bridges_) {
      // StopTracingOnUI should only be called once all bridges have completed
      // or abandoned startup.
      DCHECK_NE(ArcTracingBridge::State::kStarting, bridge->state());
      if (bridge->state() != ArcTracingBridge::State::kEnabled) {
        continue;
      }
      // |this| never gets destructed, so it's OK to bind an unretained pointer.
      bridge->StopTracing(base::BindOnce(
          &ArcTracingDataSource::OnTracingStoppedOnUI, base::Unretained(this)));
    }

    // There may not have been any bridges left in State::kEnabled. This will
    // call the callback if all bridges are already stopped.
    OnTracingStoppedOnUI();
  }

  // Called by each bridge when it has started tracing. Also called when a
  // bridge is unregisted.
  void OnTracingStartedOnUI(bool success) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (!IsAnyBridgeStarting() && pending_stop_tracing_) {
      std::move(pending_stop_tracing_).Run();
    }
  }

  // Called by each bridge when it has stopped tracing. Also called when a
  // bridge is unregisted. Records the supplied |data| into the
  // |producer_on_ui_thread_|'s buffer.
  void OnTracingStoppedOnUI() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    // When a bridge unregisters, we may not actually be stopping.
    if (!stop_complete_callback_) {
      return;
    }

    DCHECK(producer_on_ui_thread_);

    if (AreAllBridgesStopped()) {
      if (!trace_writer_) {
        OnTraceDataCommittedOnUI();
        return;
      }
      trace_writer_->Flush(
          base::BindOnce(&ArcTracingDataSource::OnTraceDataCommittedOnUI,
                         base::Unretained(this)));
    }
  }

  void OnTraceDataCommittedOnUI() {
    producer_on_ui_thread_ = nullptr;
    perfetto_task_runner_->PostTask(FROM_HERE,
                                    std::move(stop_complete_callback_));
  }

  bool IsAnyBridgeStarting() const {
    for (ArcTracingBridge* bridge : bridges_) {
      if (bridge->state() == ArcTracingBridge::State::kStarting) {
        return true;
      }
    }
    return false;
  }

  bool AreAllBridgesStopped() const {
    for (ArcTracingBridge* bridge : bridges_) {
      if (bridge->state() != ArcTracingBridge::State::kDisabled) {
        return false;
      }
    }
    return true;
  }

  scoped_refptr<base::SequencedTaskRunner> perfetto_task_runner_;
  std::set<raw_ptr<ArcTracingBridge, SetExperimental>> bridges_;
  // In case StopTracing() is called before tracing was started for all bridges,
  // this stores a callback to StopTracing() that's executed when all bridges
  // have started.
  base::OnceClosure pending_stop_tracing_;
  // Called when all bridges have completed stopping, notifying
  // PerfettoProducer.
  base::OnceClosure stop_complete_callback_;
  // Parent class's |producer_| member is only valid on the perfetto sequence,
  // we need to track it ourselves for access from the UI thread.
  raw_ptr<tracing::PerfettoProducer> producer_on_ui_thread_ = nullptr;
  perfetto::DataSourceConfig data_source_config_;
  std::unique_ptr<SystemTraceWriter> trace_writer_;
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

// static
ArcTracingBridge* ArcTracingBridge::GetForBrowserContextForTesting(
    content::BrowserContext* context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return ArcTracingBridgeFactory::GetForBrowserContextForTesting(context);
}

ArcTracingBridge::ArcTracingBridge(content::BrowserContext* context,
                                   ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service), agent_(this) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  arc_bridge_service_->tracing()->AddObserver(this);
  ArcTracingDataSource::GetInstance()->RegisterBridgeOnUI(this);
}

ArcTracingBridge::~ArcTracingBridge() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ArcTracingDataSource::GetInstance()->UnregisterBridgeOnUI(this);
  arc_bridge_service_->tracing()->RemoveObserver(this);
}

void ArcTracingBridge::GetCategories(std::set<std::string>* category_set) {
  base::AutoLock lock(categories_lock_);
  for (const auto& category : categories_) {
    category_set->insert(category.full_name);
  }
}

void ArcTracingBridge::OnConnectionReady() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  mojom::TracingInstance* tracing_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->tracing(), QueryAvailableCategories);
  if (!tracing_instance) {
    return;
  }
  tracing_instance->QueryAvailableCategories(base::BindOnce(
      &ArcTracingBridge::OnCategoriesReady, weak_ptr_factory_.GetWeakPtr()));
}

void ArcTracingBridge::OnCategoriesReady(
    const std::vector<std::string>& categories) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::AutoLock lock(categories_lock_);
  // There is no API in TraceLog to remove a category from the UI. As an
  // alternative, the old category that is no longer in |categories_| will be
  // ignored when calling |StartTracing|.
  categories_.clear();
  for (const auto& category : categories) {
    categories_.emplace_back(Category{category, kCategoryPrefix + category});
  }
}

void ArcTracingBridge::StartTracing(const std::string& config,
                                    StartCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (state_ != State::kDisabled) {
    DLOG(WARNING) << "Cannot start tracing, it is already enabled.";
    std::move(callback).Run(false /*success*/);
    return;
  }
  state_ = State::kStarting;

  base::trace_event::TraceConfig trace_config(config);

  if (!trace_config.IsSystraceEnabled()) {
    OnArcTracingStarted(std::move(callback), false /*success*/);
    return;
  }

  mojom::TracingInstance* tracing_instance =
      ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->tracing(), StartTracing);
  if (!tracing_instance) {
    OnArcTracingStarted(std::move(callback), false /*success*/);
    return;
  }

  std::vector<std::string> selected_categories;
  {
    base::AutoLock lock(categories_lock_);
    for (const auto& category : categories_) {
      if (trace_config.IsCategoryGroupEnabled(category.full_name)) {
        selected_categories.push_back(category.name);
      }
    }
  }

  tracing_instance->StartTracing(
      selected_categories, mojo::ScopedHandle(),
      base::BindOnce(&ArcTracingBridge::OnArcTracingStarted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ArcTracingBridge::OnArcTracingStarted(StartCallback callback,
                                           bool success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(State::kStarting, state_);
  state_ = success ? State::kEnabled : State::kDisabled;
  std::move(callback).Run(success);
}

void ArcTracingBridge::StopTracing(StopCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (state_ != State::kEnabled) {
    DLOG(WARNING) << "Cannot stop tracing, it is not enabled.";
    std::move(callback).Run();
    return;
  }
  state_ = State::kStopping;

  mojom::TracingInstance* tracing_instance =
      ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->tracing(), StopTracing);
  if (!tracing_instance) {
    OnArcTracingStopped(std::move(callback), false);
    return;
  }
  tracing_instance->StopTracing(
      base::BindOnce(&ArcTracingBridge::OnArcTracingStopped,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ArcTracingBridge::OnArcTracingStopped(StopCallback callback,
                                           bool success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(State::kStopping, state_);
  state_ = State::kDisabled;
  if (!success) {
    LOG(ERROR) << "Failed to stop tracing";
  }
  std::move(callback).Run();
}

ArcTracingBridge::ArcTracingAgent::ArcTracingAgent(ArcTracingBridge* bridge)
    : bridge_(bridge) {}

ArcTracingBridge::ArcTracingAgent::~ArcTracingAgent() = default;

void ArcTracingBridge::ArcTracingAgent::GetCategories(
    std::set<std::string>* category_set) {
  bridge_->GetCategories(category_set);
}

// static
void ArcTracingBridge::EnsureFactoryBuilt() {
  ArcTracingBridgeFactory::GetInstance();
}

}  // namespace arc
