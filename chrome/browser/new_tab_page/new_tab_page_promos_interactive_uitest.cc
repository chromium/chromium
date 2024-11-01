// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/interaction/webcontents_interaction_test_util.h"
#include "components/search/ntp_features.h"
#include "content/public/test/browser_test.h"

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewTabPageElementId);

using DeepQuery = WebContentsInteractionTestUtil::DeepQuery;

}  // namespace

class NewTabPagePromosInteractiveUiTest : public InteractiveBrowserTest {
 public:
  NewTabPagePromosInteractiveUiTest() = default;
  ~NewTabPagePromosInteractiveUiTest() override = default;
  NewTabPagePromosInteractiveUiTest(const NewTabPagePromosInteractiveUiTest&) =
      delete;
  void operator=(const NewTabPagePromosInteractiveUiTest&) = delete;

  void SetUp() override {
    // Force promo to appear.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kForceNtpMobilePromo);
    features_.InitAndEnableFeature(ntp_features::kNtpMobilePromo);
    InteractiveBrowserTest::SetUp();
  }

  InteractiveTestApi::MultiStep LoadNewTabPage() {
    return Steps(InstrumentTab(kNewTabPageElementId, 0),
                 NavigateWebContents(kNewTabPageElementId,
                                     GURL(chrome::kChromeUINewTabPageURL)),
                 WaitForWebContentsReady(kNewTabPageElementId,
                                         GURL(chrome::kChromeUINewTabPageURL)));
  }

  InteractiveTestApi::MultiStep WaitForElementToLoad(const DeepQuery& element) {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementLoadedEvent);
    StateChange element_loaded;
    element_loaded.event = kElementLoadedEvent;
    element_loaded.type = StateChange::Type::kExists;
    element_loaded.where = element;
    return WaitForStateChange(kNewTabPageElementId, std::move(element_loaded));
  }

  InteractiveTestApi::MultiStep WaitForElementHidden(const DeepQuery& element) {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementHiddenEvent);
    StateChange element_hidden;
    element_hidden.event = kElementHiddenEvent;
    element_hidden.type = StateChange::Type::kExistsAndConditionTrue;
    element_hidden.where = element;
    element_hidden.test_function = "(el) => { return el.hidden; }";
    return WaitForStateChange(kNewTabPageElementId, std::move(element_hidden));
  }

  InteractiveTestApi::MultiStep WaitForElementUnhidden(
      const DeepQuery& element) {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementUnhiddenEvent);
    StateChange element_unhidden;
    element_unhidden.event = kElementUnhiddenEvent;
    element_unhidden.type = StateChange::Type::kExistsAndConditionTrue;
    element_unhidden.where = element;
    element_unhidden.test_function = "(el) => { return !el.hidden; }";
    return WaitForStateChange(kNewTabPageElementId,
                              std::move(element_unhidden));
  }

  InteractiveTestApi::MultiStep ClickElement(
      const ui::ElementIdentifier& contents_id,
      const DeepQuery& element) {
    return Steps(MoveMouseTo(contents_id, element), ClickMouse());
  }

 protected:
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(NewTabPagePromosInteractiveUiTest,
                       MobilePromoDismissalAndUndo) {
  const DeepQuery kMobilePromoContainer = {"ntp-app", "ntp-middle-slot-promo",
                                           "ntp-mobile-promo",
                                           "#promoContainer"};
  const DeepQuery kMobilePromoDismissButton = {
      "ntp-app", "ntp-middle-slot-promo", "ntp-mobile-promo", "#dismissButton"};
  const DeepQuery kMobilePromoUndoDismissButton = {
      "ntp-app", "ntp-middle-slot-promo", "ntp-mobile-promo",
      "#undoDismissPromoButton"};

  RunTestSequence(
      // 1. Wait for new tab page to load.
      LoadNewTabPage(),
      // 2. Wait for mobile promo to appear.
      WaitForElementToLoad(kMobilePromoContainer),
      // 3. Scroll until the dismiss button is visible, in case the promo
      // appears below the fold.
      ScrollIntoView(kNewTabPageElementId, kMobilePromoDismissButton),
      // 4. Click the "dismiss" button for the promo.
      ClickElement(kNewTabPageElementId, kMobilePromoDismissButton),
      // 5. Wait for the mobile promo to be hidden.
      WaitForElementHidden(kMobilePromoContainer),
      // 6. Wait for the undo button to be shown.
      WaitForElementToLoad(kMobilePromoUndoDismissButton),
      // 7. Click the "undo dismiss button".
      ClickElement(kNewTabPageElementId, kMobilePromoUndoDismissButton),
      // 8. Wait for the promo to be unhidden.
      WaitForElementUnhidden(kMobilePromoContainer));
}
