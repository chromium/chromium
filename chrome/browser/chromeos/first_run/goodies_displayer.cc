// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/first_run/goodies_displayer.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"

namespace chromeos {
namespace first_run {

namespace {

GoodiesDisplayer* g_goodies_displayer = nullptr;
GoodiesDisplayerTestInfo* g_test_info = nullptr;

// Checks timestamp on OOBE Complete flag file, or use fake device age for test.
// kCanShowOobeGoodiesPage defaults to |true|; set to |false| (return |false|)
// for any device over kMaxDaysAfterOobeForGoodies days old, to avoid showing
// Goodies after update on older devices.
bool CheckGoodiesPrefAgainstOobeTimestamp() {
  const int days_since_oobe =
      g_test_info ? g_test_info->days_since_oobe
                  : StartupUtils::GetTimeSinceOobeFlagFileCreation().InDays();
  return days_since_oobe <= GoodiesDisplayer::kMaxDaysAfterOobeForGoodies;
}

// Callback into main thread to set pref to |false| if too long since oobe, or
// to create GoodiesDisplayer otherwise.
void UpdateGoodiesPrefCantShow(bool can_show_goodies) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (can_show_goodies) {
    if (!g_goodies_displayer)
      g_goodies_displayer = new GoodiesDisplayer();
  } else {
    g_browser_process->local_state()->SetBoolean(prefs::kCanShowOobeGoodiesPage,
                                                 false);
  }

  if (g_test_info) {
    g_test_info->setup_complete = true;
    if (!g_test_info->on_setup_complete_callback.is_null())
      g_test_info->on_setup_complete_callback.Run();
  }
}

}  // namespace

const char GoodiesDisplayer::kGoodiesURL[] =
    "https://www.google.com/chromebook/offers/";

GoodiesDisplayer::GoodiesDisplayer() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  BrowserList::AddObserver(this);
}

GoodiesDisplayer::~GoodiesDisplayer() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  BrowserList::RemoveObserver(this);
}

// If Goodies page hasn't been shown yet, and Chromebook isn't too old, create
// GoodiesDisplayer to observe BrowserList.  Return |true| if checking age.
// static
bool GoodiesDisplayer::Init() {
  const bool can_show = g_browser_process->local_state()->GetBoolean(
      prefs::kCanShowOobeGoodiesPage);
  if (can_show) {
    base::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::Bind(&CheckGoodiesPrefAgainstOobeTimestamp),
        base::Bind(&UpdateGoodiesPrefCantShow));
  }
  return can_show;
}

// static
void GoodiesDisplayer::InitForTesting(GoodiesDisplayerTestInfo* test_info) {
  CHECK(!g_test_info) << "GoodiesDisplayer::InitForTesting called twice";
  g_test_info = test_info;
  test_info->setup_complete = !Init();
}

// static
void GoodiesDisplayer::Delete() {
  delete g_goodies_displayer;
  g_goodies_displayer = nullptr;
}

// If conditions enumerated below are met, this loads the Oobe Goodies page for
// new Chromebooks; when appropriate, it uses pref to mark page as shown,
// removes itself from BrowserListObservers, and deletes itself.
void GoodiesDisplayer::OnBrowserSetLastActive(Browser* browser) {
  // 1. Must be an actual tabbed browser window.
  if (!browser->is_type_normal())
    return;

  // 2. Not guest or incognito session or supervised user (keep observing).
  user_manager::User* user = user_manager::UserManager::Get()->GetActiveUser();
  if (browser->profile()->IsOffTheRecord() || (user && user->IsSupervised()))
    return;

  PrefService* local_state = g_browser_process->local_state();
  // 3. Not previously shown, or otherwise marked as unavailable.
  if (local_state->GetBoolean(prefs::kCanShowOobeGoodiesPage)) {
    // 4. Device not enterprise enrolled.
    const bool enterprise_managed = g_browser_process->platform_part()
                                        ->browser_policy_connector_chromeos()
                                        ->IsEnterpriseManaged();
    // 5. --no-first-run not specified, as it is for tests. --force-run takes
    // precedence over --no-first-run.
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    const bool first_run_permitted =
        command_line->HasSwitch(switches::kForceFirstRun) ||
        !command_line->HasSwitch(switches::kNoFirstRun);

    if (!enterprise_managed && first_run_permitted)
      chrome::AddTabAt(browser, GURL(kGoodiesURL), 2, false);

    // Set to |false| whether enterprise enrolled or Goodies shown.
    local_state->SetBoolean(prefs::kCanShowOobeGoodiesPage, false);
  }

  // Regardless of how we got here, we don't henceforth need to show Goodies.
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                base::BindOnce(&Delete));
}

GoodiesDisplayerTestInfo::GoodiesDisplayerTestInfo()
    : days_since_oobe(0), setup_complete(false) {}

GoodiesDisplayerTestInfo::~GoodiesDisplayerTestInfo() {}

}  // namespace first_run
}  // namespace chromeos
