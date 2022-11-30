// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/quick_answers_menu_observer.h"

#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/branding_buildflags.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/components/quick_answers/public/cpp/controller/quick_answers_controller.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_state.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/renderer_context_menu/render_view_context_menu_proxy.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "third_party/blink/public/mojom/context_menu/context_menu.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/text_constants.h"
#include "ui/gfx/text_elider.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/lacros/feedback_util.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace {

using quick_answers::Context;
using quick_answers::QuickAnswersExitPoint;

constexpr int kMaxSurroundingTextLength = 300;

bool IsActiveUserInternal() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  auto* user = user_manager::UserManager::Get()->GetActiveUser();
  const std::string email = user->GetAccountId().GetUserEmail();
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  const std::string email = feedback_util::GetSignedInUserEmail();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  return gaia::IsGoogleInternalAccountEmail(email);
}

}  // namespace

QuickAnswersMenuObserver::QuickAnswersMenuObserver(
    RenderViewContextMenuProxy* proxy)
    : proxy_(proxy) {}

QuickAnswersMenuObserver::~QuickAnswersMenuObserver() = default;

void QuickAnswersMenuObserver::OnContextMenuShown(
    const content::ContextMenuParams& params,
    const gfx::Rect& bounds_in_screen) {
  DCHECK(QuickAnswersController::Get());
  menu_shown_time_ = base::TimeTicks::Now();

  if (!QuickAnswersState::Get()->is_eligible())
    return;

  // Skip password input field.
  if (params.input_field_type ==
      blink::mojom::ContextMenuDataInputFieldType::kPassword) {
    return;
  }

  // Skip if no text selected.
  auto selected_text = base::UTF16ToUTF8(params.selection_text);
  if (selected_text.empty())
    return;

  bounds_in_screen_ = bounds_in_screen;

  content::RenderFrameHost* focused_frame =
      proxy_->GetWebContents()->GetFocusedFrame();
  if (focused_frame) {
    QuickAnswersController::Get()->SetPendingShowQuickAnswers();
    focused_frame->RequestTextSurroundingSelection(
        base::BindOnce(
            &QuickAnswersMenuObserver::OnTextSurroundingSelectionAvailable,
            weak_factory_.GetWeakPtr(), selected_text),
        kMaxSurroundingTextLength);
  }
}

void QuickAnswersMenuObserver::OnContextMenuViewBoundsChanged(
    const gfx::Rect& bounds_in_screen) {
  bounds_in_screen_ = bounds_in_screen;
  QuickAnswersController::Get()->UpdateQuickAnswersAnchorBounds(
      bounds_in_screen);
}

void QuickAnswersMenuObserver::OnMenuClosed() {
  const base::TimeDelta time_since_request_sent =
      base::TimeTicks::Now() - menu_shown_time_;
  if (is_other_command_executed_) {
    base::UmaHistogramTimes("QuickAnswers.ContextMenu.Close.DurationWithClick",
                            time_since_request_sent);
  } else {
    base::UmaHistogramTimes(
        "QuickAnswers.ContextMenu.Close.DurationWithoutClick",
        time_since_request_sent);
  }

  base::UmaHistogramBoolean("QuickAnswers.ContextMenu.Close",
                            is_other_command_executed_);

  QuickAnswersController::Get()->DismissQuickAnswers(
      is_other_command_executed_ ? QuickAnswersExitPoint::kContextMenuClick
                                 : QuickAnswersExitPoint::KContextMenuDismiss);
}

void QuickAnswersMenuObserver::CommandWillBeExecuted(int command_id) {
  is_other_command_executed_ = true;
}

void QuickAnswersMenuObserver::OnTextSurroundingSelectionAvailable(
    const std::string& selected_text,
    const std::u16string& surrounding_text,
    uint32_t start_offset,
    uint32_t end_offset) {
  Context context;
  context.surrounding_text = base::UTF16ToUTF8(surrounding_text);
  context.device_properties.is_internal = IsActiveUserInternal();
  QuickAnswersController::Get()->MaybeShowQuickAnswers(bounds_in_screen_,
                                                       selected_text, context);
}
