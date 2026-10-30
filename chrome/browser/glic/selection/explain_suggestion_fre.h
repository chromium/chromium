// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SELECTION_EXPLAIN_SUGGESTION_FRE_H_
#define CHROME_BROWSER_GLIC_SELECTION_EXPLAIN_SUGGESTION_FRE_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/widget/widget.h"

class Profile;

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace glic {

inline constexpr char kQuickAnswersFreDialogName[] =
    "GlicQuickAnswersFreDialog";

class ExplainSuggestionFre {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kAllowButtonElementId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kCancelButtonElementId);

  using ResultCallback = base::OnceCallback<void(bool accepted)>;

  explicit ExplainSuggestionFre(tabs::TabInterface& tab);
  ExplainSuggestionFre(const ExplainSuggestionFre&) = delete;
  ExplainSuggestionFre& operator=(const ExplainSuggestionFre&) = delete;
  ~ExplainSuggestionFre();

  static bool IsAccepted(Profile* profile);

  // Shows the dialog over the tab's window, and runs `callback` when it
  // closes. Runs it with false right away if the dialog can't be shown or is
  // already showing.
  void Show(ResultCallback callback);

  bool IsShowing() const;

  // Closes the dialog without running the callback.
  void Close();

 private:
  void OnClosed(views::Widget::ClosedReason reason);

  const raw_ref<tabs::TabInterface> tab_;
  ResultCallback callback_;
  std::unique_ptr<views::Widget> widget_;
  base::WeakPtrFactory<ExplainSuggestionFre> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SELECTION_EXPLAIN_SUGGESTION_FRE_H_
