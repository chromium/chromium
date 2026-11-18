// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <unordered_map>

#include "base/command_line.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_provider_client.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/omnibox/omnibox_pedal_implementations.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/omnibox/browser/actions/omnibox_pedal.h"
#include "components/omnibox/browser/actions/omnibox_pedal_concepts.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_type.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/base/net_errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "url/gurl.h"

namespace policy {

class DinosaurEasterEggPolicyTest
    : public PolicyTest,
      public testing::WithParamInterface<PolicyTest::BooleanPolicy> {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    PolicyTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kDisableAutoReload);
  }

  void SetUpInProcessBrowserTestFixture() override {
    PolicyTest::SetUpInProcessBrowserTestFixture();
    if (GetParam() != BooleanPolicy::kNotConfigured) {
      PolicyMap policies;
      SetPolicy(&policies, key::kAllowDinosaurEasterEgg,
                base::Value(GetParam() == BooleanPolicy::kTrue));
      UpdateProviderPolicy(policies);
    }
  }

  bool IsAllowed() const { return GetParam() != BooleanPolicy::kFalse; }

  content::WebContents* GetWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  void WaitForCompositorFrame() {
    base::test::TestFuture<bool> visual_state_updated;
    GetWebContents()->GetPrimaryMainFrame()->InsertVisualStateCallback(
        visual_state_updated.GetCallback());
    EXPECT_TRUE(visual_state_updated.Get());
  }

  void PressSpace() {
    GetWebContents()->Focus();
    content::SimulateEndOfPaintHoldingOnPrimaryMainFrame(GetWebContents());
    content::RenderWidgetHost* render_widget_host =
        GetWebContents()->GetPrimaryMainFrame()->GetRenderWidgetHost();
    ASSERT_TRUE(render_widget_host);
    content::InputEventAckWaiter key_ack_waiter(
        render_widget_host, blink::WebInputEvent::Type::kKeyUp);
    content::SimulateKeyPress(GetWebContents(), ui::DomKey::FromCharacter(' '),
                              ui::DomCode::SPACE, ui::VKEY_SPACE,
                              /*control=*/false, /*shift=*/false,
                              /*alt=*/false, /*command=*/false);
    key_ack_waiter.Wait();
  }

  void VerifyDinoSurface(const GURL& url) {
    EXPECT_FALSE(content::NavigateToURL(GetWebContents(), url));
    ASSERT_TRUE(content::IsLastCommittedEntryOfPageType(
        GetWebContents(), content::PAGE_TYPE_ERROR));

    if (IsAllowed()) {
      static constexpr char kWaitForRunnerCanvasScript[] = R"JS(
        new Promise(resolve => {
          if (document.querySelector('canvas.runner-canvas')) {
            resolve(true);
            return;
          }
          const observer = new MutationObserver(() => {
            if (document.querySelector('canvas.runner-canvas')) {
              observer.disconnect();
              resolve(true);
            }
          });
          observer.observe(document.body, {childList: true, subtree: true});
        })
      )JS";
      ASSERT_EQ(true,
                content::EvalJs(GetWebContents(), kWaitForRunnerCanvasScript));
      EXPECT_EQ(true, content::EvalJs(
                          GetWebContents(),
                          "!document.querySelector('div.snackbar') && "
                          "!!document.querySelector('canvas.runner-canvas')"));
      ASSERT_NO_FATAL_FAILURE(PressSpace());
      WaitForCompositorFrame();
      return;
    }

    ASSERT_NO_FATAL_FAILURE(PressSpace());
    content::EvalJsResult eval_result = content::EvalJs(GetWebContents(), R"JS(
      (() => {
        const snackbar = document.querySelector('div.snackbar');
        const icon = document.querySelector('.icon');
        return {
          snackbarText: snackbar ? snackbar.textContent : '',
          snackbarShown:
              snackbar ? snackbar.classList.contains('snackbar-show') : false,
          iconDisabled: icon ? icon.classList.contains('icon-disabled') : false,
          hasRunnerCanvas: !!document.querySelector('canvas.runner-canvas'),
        };
      })()
    )JS");
    ASSERT_TRUE(eval_result.is_ok()) << eval_result.ExtractError();
    ASSERT_TRUE(eval_result.is_dict()) << eval_result;
    const base::DictValue& dict = eval_result.ExtractDict();
    EXPECT_THAT(dict.FindString("snackbarText"),
                testing::Pointee(testing::Eq(
                    l10n_util::GetStringUTF8(IDS_ERRORPAGE_FUN_DISABLED))));
    EXPECT_THAT(dict.FindBool("snackbarShown"), testing::Optional(true));
    EXPECT_THAT(dict.FindBool("iconDisabled"), testing::Optional(true));
    EXPECT_THAT(dict.FindBool("hasRunnerCanvas"), testing::Optional(false));
    WaitForCompositorFrame();
  }
};

IN_PROC_BROWSER_TEST_P(DinosaurEasterEggPolicyTest, AllowDinosaurEasterEgg) {
  Profile* profile = chrome_test_utils::GetProfile(this);
  PrefService* prefs = profile->GetPrefs();

  EXPECT_EQ(GetParam() != BooleanPolicy::kNotConfigured,
            prefs->IsManagedPreference(prefs::kAllowDinosaurEasterEgg));
  EXPECT_EQ(IsAllowed(), prefs->GetBoolean(prefs::kAllowDinosaurEasterEgg));

  {
    ChromeAutocompleteProviderClient autocomplete_client(profile);
    const std::unordered_map<OmniboxPedalId, scoped_refptr<OmniboxPedal>>
        pedals = GetPedalImplementations();
    const auto pedal_it = pedals.find(OmniboxPedalId::PLAY_CHROME_DINO_GAME);
    ASSERT_NE(pedal_it, pedals.end());
    EXPECT_EQ(IsAllowed(), pedal_it->second->IsReadyToTrigger(
                               AutocompleteInput(), autocomplete_client));
  }

  {
    static constexpr char kOfflineTestUrl[] = "https://offline.example/";
    std::unique_ptr<content::URLLoaderInterceptor> interceptor =
        content::URLLoaderInterceptor::SetupRequestFailForURL(
            GURL(kOfflineTestUrl), net::ERR_INTERNET_DISCONNECTED);
    ASSERT_NO_FATAL_FAILURE(VerifyDinoSurface(GURL(kOfflineTestUrl)));
  }
  ASSERT_NO_FATAL_FAILURE(VerifyDinoSurface(GURL("chrome://dino/")));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    DinosaurEasterEggPolicyTest,
    testing::Values(PolicyTest::BooleanPolicy::kNotConfigured,
                    PolicyTest::BooleanPolicy::kFalse,
                    PolicyTest::BooleanPolicy::kTrue),
    [](const testing::TestParamInfo<PolicyTest::BooleanPolicy>& info) {
      switch (info.param) {
        case PolicyTest::BooleanPolicy::kNotConfigured:
          return "NotConfigured";
        case PolicyTest::BooleanPolicy::kFalse:
          return "Disabled";
        case PolicyTest::BooleanPolicy::kTrue:
          return "Enabled";
      }
    });

}  // namespace policy
