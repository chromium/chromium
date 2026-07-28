// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/dictation_browser_test_base.h"

#include "base/command_line.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/dictation/dictation_keyed_service.h"
#include "chrome/browser/dictation/features.h"
#include "chrome/browser/dictation/listener_stream_provider.h"
#include "chrome/browser/dictation/session_controller.h"
#include "chrome/browser/dictation/target.h"
#include "chrome/browser/dictation/test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/global_dom_node_id.h"
#include "content/public/browser/render_frame_host.h"
#include "extensions/common/switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace dictation {

DictationBrowserTestBase::DictationBrowserTestBase()
    : scoped_feature_list_(CreateEnablingFeatureList()) {}

DictationBrowserTestBase::~DictationBrowserTestBase() = default;

void DictationBrowserTestBase::SetUpCommandLine(
    base::CommandLine* command_line) {
  PlatformBrowserTest::SetUpCommandLine(command_line);
  command_line->AppendSwitchASCII(extensions::switches::kAllowlistedExtensionID,
                                  kDictationTestExtensionId);
}

void DictationBrowserTestBase::SetUpOnMainThread() {
  PlatformBrowserTest::SetUpOnMainThread();
  embedded_test_server()->ServeFilesFromSourceDirectory(
      "components/test/data");
  ASSERT_TRUE(embedded_test_server()->Start());
  profile()->GetPrefs()->SetBoolean(prefs::kPrefDictationOnboardingCompleted,
                                    true);
  LoadTestExtensionInManualMode(profile());
}

Profile* DictationBrowserTestBase::profile() {
  return chrome_test_utils::GetProfile(this);
}

content::WebContents* DictationBrowserTestBase::web_contents() {
  return chrome_test_utils::GetActiveWebContents(this);
}

DictationKeyedService& DictationBrowserTestBase::dictation_service() {
  return *DictationKeyedService::Get(profile());
}

SessionController* DictationBrowserTestBase::session_controller() {
  return dictation_service().session_controller();
}

ListenerStreamProvider* DictationBrowserTestBase::attached_stream() {
  return static_cast<ListenerStreamProvider*>(
      session_controller()->attached_stream_provider());
}

void DictationBrowserTestBase::StartSession(
    const TargetDetails& target_details) {
  tabs::TabInterface* tab = chrome_test_utils::GetActiveTab(this);
  CHECK(tab);
  dictation_service().StartSession(*tab, target_details,
                                   DictationSessionEntryPoint::kContextMenu);
}

void DictationBrowserTestBase::StartSession() {
  StartSession(DefaultInPageTarget(web_contents()));
}

void DictationBrowserTestBase::SimulateInvokeViaContextMenu(
    content::RenderFrameHost* render_frame_host,
    blink::DOMNodeIdType node_id) {
  CHECK(render_frame_host);
  content::ContextMenuParams params;
  params.is_editable = true;
  params.form_field_dom_node_id = content::GlobalDOMNodeId(
      render_frame_host->GetWeakDocumentPtr(), node_id);
  TestRenderViewContextMenu menu(*render_frame_host, params);
  menu.Init();
  // Ensure we only invoke the item when the context menu should be available.
  ASSERT_TRUE(menu.IsItemEnabled(IDC_CONTENT_CONTEXT_DICTATION));
  menu.ExecuteCommand(IDC_CONTENT_CONTEXT_DICTATION, 0);
}

}  // namespace dictation
