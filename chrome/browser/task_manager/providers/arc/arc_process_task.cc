// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/arc/arc_process_task.h"

#include <utility>

#include "base/bind.h"
#include "base/i18n/rtl.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "chrome/grit/generated_resources.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/arc_util.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "components/arc/mojom/process.mojom.h"
#include "components/arc/session/arc_bridge_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/child_process_host.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"

namespace task_manager {

namespace {

// |arc_process_.packages()| contains an alphabetically-sorted list of
// package names the process has. Since the Task class can hold only one
// icon per process, and there is no reliable way to pick the most important
// process from the |arc_process_.packages()| list, just use the first item
// in the list.  In some case, |arc_process_.packages()| is empty, it would
// be expected to get default process icon. For example, daemon processes in
// android container such like surfaceflinger, debuggerd or installd. Each
// of them would be shown on task manager but does not have a package name.
std::string FirstPackage(const std::vector<std::string>& packages) {
  return packages.empty() ? std::string() : packages[0];
}

base::string16 MakeTitle(const arc::ArcProcess& arc_process) {
  int name_template = IDS_TASK_MANAGER_ARC_PREFIX;
  switch (arc_process.process_state()) {
    case arc::mojom::ProcessState::PERSISTENT:
    case arc::mojom::ProcessState::PERSISTENT_UI:
      name_template = IDS_TASK_MANAGER_ARC_SYSTEM;
      break;
    case arc::mojom::ProcessState::FOREGROUND_SERVICE:
    case arc::mojom::ProcessState::BOUND_FOREGROUND_SERVICE:
    case arc::mojom::ProcessState::IMPORTANT_FOREGROUND:
    case arc::mojom::ProcessState::IMPORTANT_BACKGROUND:
    case arc::mojom::ProcessState::TRANSIENT_BACKGROUND:
    case arc::mojom::ProcessState::SERVICE:
      name_template = IDS_TASK_MANAGER_ARC_PREFIX_BACKGROUND_SERVICE;
      break;
    case arc::mojom::ProcessState::RECEIVER:
      name_template = IDS_TASK_MANAGER_ARC_PREFIX_RECEIVER;
      break;
    default:
      break;
  }
  base::string16 title = l10n_util::GetStringFUTF16(
      name_template, base::UTF8ToUTF16(arc_process.process_name()));
  base::i18n::AdjustStringForLocaleDirection(&title);
  return title;
}

// An activity name for retrieving the package's default icon without
// specifying an activity name.
constexpr char kEmptyActivityName[] = "";

}  // namespace

ArcProcessTask::ArcProcessTask(arc::ArcProcess arc_process)
    : Task(MakeTitle(arc_process),
           arc_process.process_name(),
           nullptr /* icon */,
           arc_process.pid()),
      arc_process_(std::move(arc_process)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  StartIconLoading();
}

void ArcProcessTask::StartIconLoading() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // TaskManager is not tied to BrowserContext. Thus, we just use the
  // BrowserContext which is tied to ARC.
  auto* arc_service_manager = arc::ArcServiceManager::Get();
  auto* intent_helper_bridge = arc::ArcIntentHelperBridge::GetForBrowserContext(
      arc_service_manager->browser_context());
  arc::ArcIntentHelperBridge::GetResult result =
      arc::ArcIntentHelperBridge::GetResult::FAILED_ARC_NOT_READY;
  if (intent_helper_bridge) {
    std::vector<arc::ArcIntentHelperBridge::ActivityName> activities = {
        {FirstPackage(arc_process_.packages()), kEmptyActivityName}};
    result = intent_helper_bridge->GetActivityIcons(
        activities, base::BindOnce(&ArcProcessTask::OnIconLoaded,
                                   weak_ptr_factory_.GetWeakPtr()));
  }

  if (result == arc::ArcIntentHelperBridge::GetResult::FAILED_ARC_NOT_READY) {
    // Need to retry loading the icon.
    arc_service_manager->arc_bridge_service()->intent_helper()->AddObserver(
        this);
  }
}

ArcProcessTask::~ArcProcessTask() {
  auto* service_manager = arc::ArcServiceManager::Get();
  // This destructor can also be called when TaskManagerImpl is destructed.
  // Since TaskManagerImpl is a LAZY_INSTANCE, arc::ArcServiceManager may have
  // already been destructed. In that case, arc_bridge_service() has also been
  // destructed, and it is safe to just return.
  if (!service_manager)
    return;
  service_manager->arc_bridge_service()->intent_helper()->RemoveObserver(this);
}

Task::Type ArcProcessTask::GetType() const {
  return Task::ARC;
}

int ArcProcessTask::GetChildProcessUniqueID() const {
  // ARC process is not a child process of the browser.
  return content::ChildProcessHost::kInvalidUniqueID;
}

bool ArcProcessTask::IsKillable() {
  // Do not kill persistent processes.
  return !arc_process_.IsPersistent();
}

bool ArcProcessTask::IsRunningInVM() const {
  return arc::IsArcVmEnabled();
}

void ArcProcessTask::Kill() {
  auto* process_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc::ArcServiceManager::Get()->arc_bridge_service()->process(),
      KillProcess);
  if (!process_instance)
    return;
  process_instance->KillProcess(arc_process_.nspid(),
                                "Killed manually from Task Manager");
}

void ArcProcessTask::OnConnectionReady() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  VLOG(2) << "intent_helper instance is ready. Fetching the icon for "
          << FirstPackage(arc_process_.packages());
  arc::ArcServiceManager::Get()
      ->arc_bridge_service()
      ->intent_helper()
      ->RemoveObserver(this);

  // Instead of calling into StartIconLoading() directly, return to the main
  // loop first to make sure other ArcBridgeService observers are notified.
  // Otherwise, arc::ArcIntentHelperBridge::GetActivityIcon() may fail again.
  base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                 base::BindOnce(&ArcProcessTask::StartIconLoading,
                                weak_ptr_factory_.GetWeakPtr()));
}

void ArcProcessTask::SetProcessState(arc::mojom::ProcessState process_state) {
  arc_process_.set_process_state(process_state);
}

void ArcProcessTask::OnIconLoaded(
    std::unique_ptr<arc::ArcIntentHelperBridge::ActivityToIconsMap> icons) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  for (const auto& kv : *icons) {
    const gfx::Image& icon = kv.second.icon16;
    if (icon.IsEmpty())
      continue;
    set_icon(*icon.ToImageSkia());
    break;  // Since the parent class can hold only one icon, break here.
  }
}

}  // namespace task_manager
