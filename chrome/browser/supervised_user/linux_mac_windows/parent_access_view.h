// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_LINUX_MAC_WINDOWS_PARENT_ACCESS_VIEW_H_
#define CHROME_BROWSER_SUPERVISED_USER_LINUX_MAC_WINDOWS_PARENT_ACCESS_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/supervised_user/core/browser/supervised_user_utils.h"
#include "ui/views/view.h"

namespace views {
class WebView;
}  // namespace views

namespace content {
class WebContents;
class BrowserContext;
}  // namespace content

using WebContentsObserverCreationCallback =
    base::OnceCallback<void(content::WebContents*)>;

// Implements a View to display the Parent Access Widget (PACP).
// The view contains a WebView which loads the PACP url.
class ParentAccessView : public views::View {
  METADATA_HEADER(ParentAccessView, views::View)

 public:
  explicit ParentAccessView(content::BrowserContext* context);
  ~ParentAccessView() override;

  // Creates and opens a view that displays the Parent Access widget (PACP).
  static base::WeakPtr<ParentAccessView> ShowParentAccessDialog(
      content::WebContents* web_contents,
      const GURL& target_url,
      const supervised_user::FilteringBehaviorReason& filtering_reason,
      WebContentsObserverCreationCallback web_contents_observer_creation_cb);

  base::WeakPtr<ParentAccessView> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // Closes the widget that hosts this view.
  // Results in destructing the present view and its widget.
  void CloseView();

 private:
  // Initialize ParentAccessView's web_view_ element.
  void Initialize(const GURL& pacp_url, int corner_radius);
  void ShowNativeView();
  content::WebContents* GetWebViewContents();

  bool is_initialized_ = false;
  int corner_radius_ = 0;
  raw_ptr<views::WebView> web_view_ = nullptr;
  base::WeakPtrFactory<ParentAccessView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_LINUX_MAC_WINDOWS_PARENT_ACCESS_VIEW_H_
