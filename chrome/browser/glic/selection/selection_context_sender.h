// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SELECTION_SELECTION_CONTEXT_SENDER_H_
#define CHROME_BROWSER_GLIC_SELECTION_SELECTION_CONTEXT_SENDER_H_

#include <string>
#include <string_view>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/types/pass_key.h"
#include "chrome/browser/selection/page_selection.h"
#include "components/optimization_guide/content/browser/page_context_eligibility_observer.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace content {
class WebContents;
}  // namespace content

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace glic {

class GlicKeyedService;

class GlicSelectionObserver;

// Tab-scoped. The only code that sends this tab's selection to glic.
//
// Providers reach it only through `selection::PageSelection`. Only
// `GlicSelectionObserver`, which reports the page's selection, gets the
// concrete class.
class SelectionContextSender : public ::selection::PageSelection {
 public:
  DECLARE_SAFE_CAST_TARGET()

  static SelectionContextSender* From(base::PassKey<GlicSelectionObserver>,
                                      tabs::TabInterface* tab);

  explicit SelectionContextSender(tabs::TabInterface& tab);
  SelectionContextSender(const SelectionContextSender&) = delete;
  SelectionContextSender& operator=(const SelectionContextSender&) = delete;
  ~SelectionContextSender() override;

  // Updates the tab's current selected text. Empty means none.
  void SetText(std::u16string_view text);

  // Records that `text` was already sent to the panel by an entry point that
  // invoked glic directly (e.g. the context menu).
  void MarkTextSent(std::u16string_view text);

  // selection::PageSelection:
  const std::u16string& GetText() const override;

  bool IsPanelShowing() const;

  bool has_sent_selection_context() const {
    return has_sent_selection_context_;
  }

 private:
  void OnGlobalPanelShowHide();
  void OnPageContextEligibilityChanged(
      optimization_guide::PageContextEligibilityStatus status);
  void WillDiscardContents(tabs::TabInterface* tab,
                           content::WebContents* old_contents,
                           content::WebContents* new_contents);
  // Sends `text_` to the panel if it's showing, or clears what was sent.
  void Sync();
  // Returns whether the context was sent (or an invocation started).
  bool Push(const std::u16string& selected_text);
  bool IsPageContextEligible() const;

  // Safe: owned by `TabFeatures`, which is destroyed before `tab_`.
  const raw_ref<tabs::TabInterface> tab_;
  // Safe: KeyedService on `tab_->GetProfile()`, outlives all tabs.
  raw_ptr<GlicKeyedService> glic_keyed_service_;

  std::u16string text_;
  bool has_sent_selection_context_ = false;

  base::CallbackListSubscription panel_state_subscription_;
  base::CallbackListSubscription page_context_eligibility_subscription_;
  base::CallbackListSubscription discard_contents_subscription_;

  ui::ScopedUnownedUserData<::selection::PageSelection>
      scoped_page_selection_user_data_;
  base::WeakPtrFactory<SelectionContextSender> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SELECTION_SELECTION_CONTEXT_SENDER_H_
