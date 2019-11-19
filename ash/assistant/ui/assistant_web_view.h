// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_ASSISTANT_WEB_VIEW_H_
#define ASH_ASSISTANT_UI_ASSISTANT_WEB_VIEW_H_

#include <map>
#include <memory>
#include <string>

#include "ash/assistant/model/assistant_ui_model_observer.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/assistant/ui/caption_bar.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/optional.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/content/public/cpp/navigable_contents.h"
#include "ui/views/view.h"

namespace ash {

enum class AssistantButtonId;
class AssistantWebViewDelegate;

// AssistantWebView is a child of AssistantBubbleView which allows Assistant UI
// to render remotely hosted content within its bubble. It provides a CaptionBar
// for window level controls and embeds web contents with help from the Content
// Service.
class COMPONENT_EXPORT(ASSISTANT_UI) AssistantWebView
    : public views::View,
      public AssistantViewDelegateObserver,
      public CaptionBarDelegate,
      public content::NavigableContentsObserver,
      public AssistantUiModelObserver {
 public:
  AssistantWebView(AssistantViewDelegate* assistant_view_delegate,
                   AssistantWebViewDelegate* web_container_view_delegate);
  ~AssistantWebView() override;

  // views::View:
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;
  int GetHeightForWidth(int width) const override;
  void ChildPreferredSizeChanged(views::View* child) override;
  void OnFocus() override;
  void AboutToRequestFocusFromTabTraversal(bool reverse) override;

  // CaptionBarDelegate:
  bool OnCaptionButtonPressed(AssistantButtonId id) override;

  // AssistantViewDelegateObserver:
  void OnDeepLinkReceived(
      assistant::util::DeepLinkType type,
      const std::map<std::string, std::string>& params) override;

  // content::NavigableContentsObserver:
  void DidStopLoading() override;
  void DidSuppressNavigation(const GURL& url,
                             WindowOpenDisposition disposition,
                             bool from_user_gesture) override;
  void UpdateCanGoBack(bool can_go_back) override;

  // AssistantUiModelObserver:
  void OnUiVisibilityChanged(
      AssistantVisibility new_visibility,
      AssistantVisibility old_visibility,
      base::Optional<AssistantEntryPoint> entry_point,
      base::Optional<AssistantExitPoint> exit_point) override;
  void OnUsableWorkAreaChanged(const gfx::Rect& usable_work_area) override;

  views::View* caption_bar_for_testing() { return caption_bar_; }

 private:
  void InitLayout();
  void RemoveContents();

  // Updates the size of the web contents by changing its view size to avoid
  // either being cut or not fully filling the whole container when the usable
  // work area changed.
  void UpdateContentSize();

  // TODO(b/143177141): Remove AssistantViewDelegate once standalone is
  // deprecated.
  AssistantViewDelegate* const assistant_view_delegate_;
  AssistantWebViewDelegate* const web_container_view_delegate_;

  CaptionBar* caption_bar_ = nullptr;  // Owned by view hierarchy.

  mojo::Remote<content::mojom::NavigableContentsFactory> contents_factory_;
  std::unique_ptr<content::NavigableContents> contents_;

  bool contents_view_initialized_ = false;

  base::WeakPtrFactory<AssistantWebView> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AssistantWebView);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_ASSISTANT_WEB_VIEW_H_
