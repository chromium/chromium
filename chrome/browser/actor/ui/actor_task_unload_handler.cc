// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/actor_task_unload_handler.h"

#include <memory>

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_tab_close_skip_beforeunload_user_data.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/tabs/public/tab_interface.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/color/color_id.h"
#include "ui/events/event.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace actor {

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ActorTaskTabCloseConfirmDialog, kViewId);

namespace {

// Layout constants tailored to match macOS native alert sheet proportions.
constexpr int kLeaveSiteDialogWidth = 250;
constexpr int kProductIconSize = 48;
constexpr int kIconBadgeCornerRadius = 12;
constexpr float kDialogButtonCornerRadius = 8.0f;
constexpr int kIconBadgeBorderThickness = 1;
constexpr int kIconBadgeInsidePadding = 1;
constexpr SkColor kIconBadgeBorderColor = SkColorSetARGB(35, 0, 0, 0);

class ActorTaskTabCloseConfirmDialogDelegate
    : public ::ui::DialogModelDelegate {
 public:
  ActorTaskTabCloseConfirmDialogDelegate(
      std::unique_ptr<tabs::ScopedTabModalUI> scoped_tab_modal_ui,
      ActorTaskTabCloseConfirmDialog::CloseCallback callback)
      : scoped_tab_modal_ui_(std::move(scoped_tab_modal_ui)),
        callback_(std::move(callback)) {}

  void OnAccept() {
    scoped_tab_modal_ui_.reset();
    if (callback_) {
      std::move(callback_).Run(true);
    }
  }

  void OnCancel() {
    scoped_tab_modal_ui_.reset();
    if (callback_) {
      std::move(callback_).Run(false);
    }
  }

  void OnClose() {
    scoped_tab_modal_ui_.reset();
    if (callback_) {
      std::move(callback_).Run(false);
    }
  }

  base::WeakPtr<ActorTaskTabCloseConfirmDialogDelegate> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  std::unique_ptr<tabs::ScopedTabModalUI> scoped_tab_modal_ui_;
  ActorTaskTabCloseConfirmDialog::CloseCallback callback_;
  base::WeakPtrFactory<ActorTaskTabCloseConfirmDialogDelegate>
      weak_ptr_factory_{this};
};
bool g_always_show_for_testing = false;
bool g_suppress_for_testing = false;

void SkipSubpixelRenderingOpacityCheck(views::View* view) {
  if (auto* label = views::AsViewClass<views::Label>(view)) {
    label->SetSkipSubpixelRenderingOpacityCheck(true);
  }
  for (views::View* child : view->children()) {
    SkipSubpixelRenderingOpacityCheck(child);
  }
}
}  // namespace

// static
void ActorTaskTabCloseConfirmDialog::
    SetShowAlwaysReturnsTrueForTesting(  // IN-TEST
        bool always_show) {
  g_always_show_for_testing = always_show;
}

// static
bool ActorTaskTabCloseConfirmDialog::ShouldAlwaysShowForTesting() {
  return g_always_show_for_testing;
}

// static
void ActorTaskTabCloseConfirmDialog::SetSuppressForTesting(bool suppress) {
  g_suppress_for_testing = suppress;
}

// static
bool ActorTaskTabCloseConfirmDialog::ShouldSuppressForTesting() {
  return g_suppress_for_testing;
}

// static
bool ActorTaskTabCloseConfirmDialog::ShouldShow(
    content::WebContents* web_contents) {
  if (g_suppress_for_testing) {
    return false;
  }
  if (!base::FeatureList::IsEnabled(features::kGlicConfirmTabClose)) {
    return false;
  }
#if !BUILDFLAG(IS_ANDROID)
  tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(web_contents);
  if (!tab || !tab->GetBrowserWindowInterface() ||
      !tab->GetBrowserWindowInterface()->GetProfile()) {
    return false;
  }
  if (g_always_show_for_testing ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          "always-show-actor-unload-dialog")) {
    return true;
  }
  auto* actor_service = actor::ActorKeyedService::Get(
      tab->GetBrowserWindowInterface()->GetProfile());
  bool is_active = actor_service && actor_service->IsActiveOnTab(*tab);
  bool has_task = is_active && (actor_service->GetTaskFromTab(*tab) != nullptr);
  if (has_task) {
    return true;
  }
#endif
  return false;
}

// static
std::unique_ptr<views::Widget>
ActorTaskTabCloseConfirmDialog::ShowModalIfActuating(
    content::WebContents* web_contents,
    ActorTaskTabCloseConfirmDialog::CloseCallback callback) {
  std::unique_ptr<views::BubbleDialogModelHost> delegate =
      CreateDelegateIfActuating(web_contents, std::move(callback));
  if (!delegate) {
    return nullptr;
  }
  views::BubbleDialogModelHost* raw_delegate = delegate.release();
  raw_delegate->SetOwnershipOfNewWidget(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET);

  std::unique_ptr<views::Widget> widget =
      constrained_window::ShowWebModalDialogViewsOwned(
          raw_delegate, web_contents,
          views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  if (widget) {
    SkipSubpixelRenderingOpacityCheck(widget->GetRootView());
    if (auto* ok_btn = raw_delegate->GetOkButton()) {
      ok_btn->SetCornerRadius(kDialogButtonCornerRadius);
    }
    if (auto* cancel_btn =
            static_cast<views::MdTextButton*>(raw_delegate->GetExtraView())) {
      cancel_btn->SetCornerRadius(kDialogButtonCornerRadius);
    }
  }
  return widget;
}

// static
std::unique_ptr<views::BubbleDialogModelHost>
ActorTaskTabCloseConfirmDialog::CreateDelegateIfActuating(
    content::WebContents* web_contents,
    ActorTaskTabCloseConfirmDialog::CloseCallback callback) {
  if (!ShouldShow(web_contents)) {
    return nullptr;
  }

#if !BUILDFLAG(IS_ANDROID)
  tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(web_contents);
  if (!tab || !tab->GetBrowserWindowInterface() ||
      !tab->GetBrowserWindowInterface()->GetProfile()) {
    return nullptr;
  }
  auto* actor_service = actor::ActorKeyedService::Get(
      tab->GetBrowserWindowInterface()->GetProfile());

  actor::TaskId task_id;
  if (actor_service) {
    if (actor::ActorTask* task = actor_service->GetTaskFromTab(*tab)) {
      task_id = task->id();
    }
  }

  // If the tab is not active in the browser, activate it to show the prompt.
  if (tab->GetBrowserWindowInterface()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](base::WeakPtr<tabs::TabInterface> weak_tab) {
              if (weak_tab && weak_tab->GetBrowserWindowInterface() &&
                  weak_tab->GetContents()) {
                TabStripModel* tsm =
                    weak_tab->GetBrowserWindowInterface()->GetTabStripModel();
                int idx = tsm->GetIndexOfWebContents(weak_tab->GetContents());
                if (idx != TabStripModel::kNoTab) {
                  tsm->ActivateTabAt(idx);
                }
              }
            },
            tab->GetWeakPtr()));
  }

  auto stop_task_and_run_callback = base::BindOnce(
      [](base::WeakPtr<actor::ActorKeyedService> service, actor::TaskId id,
         base::OnceCallback<void(bool)> original_callback, bool accepted) {
        if (accepted && service && !id.is_null()) {
          service->StopTask(id,
                            actor::ActorTask::StoppedReason::kStoppedByUser);
        }
        std::move(original_callback).Run(accepted);
      },
      actor_service ? actor_service->GetWeakPtr() : nullptr, task_id,
      std::move(callback));

  return CreateDelegate(web_contents, std::move(stop_task_and_run_callback));
#else
  return nullptr;
#endif
}

// static
std::unique_ptr<views::BubbleDialogModelHost>
ActorTaskTabCloseConfirmDialog::CreateDelegate(
    content::WebContents* web_contents,
    ActorTaskTabCloseConfirmDialog::CloseCallback callback) {
  tabs::TabInterface* tab = tabs::TabInterface::GetFromContents(web_contents);
  if (!tab || !tab->CanShowModalUI()) {
    return nullptr;
  }

  auto delegate = std::make_unique<ActorTaskTabCloseConfirmDialogDelegate>(
      tab->ShowModalUI(), std::move(callback));
  auto delegate_weak_ptr = delegate->AsWeakPtr();

  const int vertical_spacing = views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_VERTICAL);
  const int dialog_width = kLeaveSiteDialogWidth;

  auto container = std::make_unique<views::BoxLayoutView>();
  container->SetOrientation(views::LayoutOrientation::kVertical);
  container->SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kCenter);
  container->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  container->SetBetweenChildSpacing(vertical_spacing);

  auto icon_container = std::make_unique<views::BoxLayoutView>();
  icon_container->SetOrientation(views::LayoutOrientation::kHorizontal);
  icon_container->SetMainAxisAlignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
  icon_container->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  icon_container->SetInsideBorderInsets(gfx::Insets(kIconBadgeInsidePadding));
  icon_container->SetBackground(views::CreateRoundedRectBackground(
      ::ui::kColorDialogBackground, kIconBadgeCornerRadius));
  icon_container->SetBorder(views::CreateRoundedRectBorder(
      kIconBadgeBorderThickness, kIconBadgeCornerRadius,
      kIconBadgeBorderColor));

  auto icon = std::make_unique<views::ImageView>();
  icon->SetImage(::ui::ImageModel::FromVectorIcon(
      vector_icons::kProductIcon, ::ui::kColorIcon, kProductIconSize));
  icon_container->AddChildView(std::move(icon));
  container->AddChildView(std::move(icon_container));

  auto title_label = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_ACTOR_LEAVE_SITE_DIALOG_TITLE),
      views::style::CONTEXT_DIALOG_TITLE, views::style::STYLE_BODY_1_BOLD);
  title_label->SetMultiLine(true);
  title_label->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  title_label->SetMaximumWidth(dialog_width);
  container->AddChildView(std::move(title_label));

  auto desc_label = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_ACTOR_LEAVE_SITE_DIALOG_DESCRIPTION),
      views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_BODY_4);
  desc_label->SetMultiLine(true);
  desc_label->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  desc_label->SetMaximumWidth(dialog_width);
  desc_label->SetSkipSubpixelRenderingOpacityCheck(true);
  container->AddChildView(std::move(desc_label));

  auto dialog_model =
      ::ui::DialogModel::Builder(std::move(delegate))
          .SetAccessibleTitle(
              l10n_util::GetStringUTF16(IDS_ACTOR_LEAVE_SITE_DIALOG_TITLE))
          .AddCustomField(
              std::make_unique<views::BubbleDialogModelHost::CustomView>(
                  std::move(container),
                  views::BubbleDialogModelHost::FieldType::kControl),
              kViewId)
          .AddExtraButton(
              base::BindRepeating(
                  [](base::WeakPtr<ActorTaskTabCloseConfirmDialogDelegate> del,
                     const ::ui::Event& event) {
                    if (del) {
                      del->OnCancel();
                    }
                  },
                  delegate_weak_ptr),
              ::ui::DialogModel::Button::Params().SetLabel(
                  l10n_util::GetStringUTF16(IDS_APP_CANCEL)))
          .AddOkButton(
              base::BindOnce(&ActorTaskTabCloseConfirmDialogDelegate::OnAccept,
                             delegate_weak_ptr),
              ::ui::DialogModel::Button::Params()
                  .SetLabel(l10n_util::GetStringUTF16(
                      IDS_ACTOR_LEAVE_SITE_DIALOG_LEAVE_SITE))
                  .SetStyle(::ui::ButtonStyle::kProminent))
          .SetDialogDestroyingCallback(
              base::BindOnce(&ActorTaskTabCloseConfirmDialogDelegate::OnClose,
                             delegate_weak_ptr))
          .Build();

  auto host = views::BubbleDialogModelHost::CreateModal(
      std::move(dialog_model), ::ui::mojom::ModalType::kChild);
  host->set_fixed_width(0);
  return host;
}

ActorTaskUnloadHandler::ActorTaskUnloadHandler() = default;

ActorTaskUnloadHandler::~ActorTaskUnloadHandler() = default;

bool ActorTaskUnloadHandler::ShouldSkipBeforeUnload(
    content::WebContents* contents) {
  return actor::ActorTabCloseSkipBeforeUnloadUserData::FromWebContents(
             contents) != nullptr;
}

bool ActorTaskUnloadHandler::ShouldShowCustomConfirmation(
    content::WebContents* contents) {
  return ActorTaskTabCloseConfirmDialog::ShouldShow(contents);
}

bool ActorTaskUnloadHandler::ShowCustomConfirmation(
    content::WebContents* contents,
    base::OnceCallback<void(bool)> on_closed) {
  auto stop_and_create_tag_callback = base::BindOnce(
      [](base::WeakPtr<ActorTaskUnloadHandler> handler,
         base::WeakPtr<content::WebContents> contents,
         base::OnceCallback<void(bool)> on_closed, bool confirmed) {
        if (confirmed && contents) {
          // Tag the WebContents so that subsequent unload checks skip showing
          // duplicate custom confirmation dialogs or standard website prompts.
          actor::ActorTabCloseSkipBeforeUnloadUserData::CreateForWebContents(
              contents.get());
        }
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(
                           [](base::WeakPtr<ActorTaskUnloadHandler> handler) {
                             if (handler) {
                               handler->owned_widget_.reset();
                             }
                           },
                           handler));
        std::move(on_closed).Run(confirmed);
      },
      weak_factory_.GetWeakPtr(), contents ? contents->GetWeakPtr() : nullptr,
      std::move(on_closed));
  owned_widget_ = ActorTaskTabCloseConfirmDialog::ShowModalIfActuating(
      contents, std::move(stop_and_create_tag_callback));
  if (owned_widget_) {
    active_widget_ = owned_widget_->GetWeakPtr();
    owned_widget_->MakeCloseSynchronous(base::BindOnce(
        [](base::WeakPtr<ActorTaskUnloadHandler> handler,
           views::Widget::ClosedReason reason) {
          if (handler) {
            base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE,
                base::BindOnce(
                    [](base::WeakPtr<ActorTaskUnloadHandler> handler) {
                      if (handler) {
                        handler->owned_widget_.reset();
                      }
                    },
                    handler));
          }
        },
        weak_factory_.GetWeakPtr()));
    return true;
  }
  return false;
}

views::Widget* ActorTaskUnloadHandler::GetActiveDialogWidgetForTesting() const {
  return active_widget_.get();
}

}  // namespace actor
