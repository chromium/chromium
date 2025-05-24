// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSAGE_EMBEDDINGS_OMNIBOX_FOCUS_CHANGE_LISTENER_H_
#define CHROME_BROWSER_PASSAGE_EMBEDDINGS_OMNIBOX_FOCUS_CHANGE_LISTENER_H_

#include <map>
#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view_observer.h"

namespace views {
class View;
}  // namespace views

class BrowserList;

using OmniboxFocusChangedCallback =
    base::RepeatingCallback<void(bool is_focused)>;

class SingleOmniboxFocusChangeListener : public views::FocusChangeListener {
 public:
  SingleOmniboxFocusChangeListener(
      views::View* omnibox_view,
      OmniboxFocusChangedCallback focus_changed_callback);
  ~SingleOmniboxFocusChangeListener() override;

  SingleOmniboxFocusChangeListener(const SingleOmniboxFocusChangeListener&) =
      delete;
  SingleOmniboxFocusChangeListener& operator=(
      const SingleOmniboxFocusChangeListener&) = delete;

  // views::FocusChangeListener:
  void OnDidChangeFocus(views::View* focused_before,
                        views::View* focused_now) override;

 private:
  raw_ptr<views::View> const omnibox_view_;
  OmniboxFocusChangedCallback focus_changed_callback_;

  base::ScopedObservation<views::FocusManager, SingleOmniboxFocusChangeListener>
      observation_{this};
};

namespace base {

template <>
struct ScopedObservationTraits<views::FocusManager,
                               SingleOmniboxFocusChangeListener> {
  static void AddObserver(views::FocusManager* source,
                          SingleOmniboxFocusChangeListener* observer) {
    source->AddFocusChangeListener(observer);
  }
  static void RemoveObserver(views::FocusManager* source,
                             SingleOmniboxFocusChangeListener* observer) {
    source->RemoveFocusChangeListener(observer);
  }
};

}  // namespace base

class OmniboxFocusChangedListener : public BrowserListObserver,
                                    public views::ViewObserver {
 public:
  OmniboxFocusChangedListener(
      OmniboxFocusChangedCallback focus_changed_callback);
  ~OmniboxFocusChangedListener() override;

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override;

  // views::ViewObserver:
  void OnViewIsDeleting(views::View* observed_view) override;

 private:
  OmniboxFocusChangedCallback focus_changed_callback_;

  // Holds a focus listener for each browser's omnibox for the lifetime of the
  // browser.
  std::map<views::View*, std::unique_ptr<SingleOmniboxFocusChangeListener>>
      single_omnibox_focus_change_listeners_;

  base::ScopedObservation<BrowserList, OmniboxFocusChangedListener>
      browser_list_observation_{this};

  base::ScopedMultiSourceObservation<views::View, OmniboxFocusChangedListener>
      omnibox_view_observation_{this};
};

#endif  // CHROME_BROWSER_PASSAGE_EMBEDDINGS_OMNIBOX_FOCUS_CHANGE_LISTENER_H_
