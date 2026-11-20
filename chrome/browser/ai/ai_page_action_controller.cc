// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_page_action_controller.h"

#include <memory>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/optimization_guide/model_execution/optimization_guide_global_state.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/dialogs/browser_dialogs.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/page_action/page_action_controller.h"
#include "chrome/browser/ui/webui/settings/on_device_ai_settings_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/optimization_guide/core/model_execution/on_device_model_names.h"
#include "components/optimization_guide/core/model_execution/on_device_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/tabs/public/tab_interface.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/actions/actions.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/models/dialog_model_field.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"

BASE_FEATURE(kOnDeviceAiPageAction, base::FEATURE_ENABLED_BY_DEFAULT);

DEFINE_ELEMENT_IDENTIFIER_VALUE(kOnDeviceAiBubbleElementId);

namespace {

using optimization_guide::OnDeviceBaseModel;
using optimization_guide::OnDeviceModelControl;

// Persisted per model in `kOnDeviceAiPageActionState`; do not renumber.
enum class PageActionState {
  kNotEligible = 0,
  kPending = 1,
  kShown = 2,
  kMaxValue = kShown,
};

OnDeviceModelControl* GetModelControl() {
  if (!g_browser_process || !g_browser_process->GetFeatures() ||
      !g_browser_process->GetFeatures()->optimization_guide_global_feature()) {
    return nullptr;
  }
  return g_browser_process->GetFeatures()
      ->optimization_guide_global_feature()
      ->Get()
      .on_device_capability()
      .GetModelControl();
}

PageActionState GetPageActionState(const PrefService& local_state,
                                   OnDeviceBaseModel model) {
  const std::string model_name =
      optimization_guide::ConvertBaseModelToUmaModelName(model);
  if (model_name == "Unknown") {
    return PageActionState::kNotEligible;
  }
  const int value =
      local_state
          .GetDict(optimization_guide::model_execution::prefs::localstate::
                       kOnDeviceAiPageActionState)
          .FindInt(model_name)
          .value_or(static_cast<int>(PageActionState::kNotEligible));
  if (value < static_cast<int>(PageActionState::kNotEligible) ||
      value > static_cast<int>(PageActionState::kMaxValue)) {
    return PageActionState::kNotEligible;
  }
  return static_cast<PageActionState>(value);
}

void SetPageActionState(PrefService& local_state,
                        OnDeviceBaseModel model,
                        PageActionState state) {
  const std::string model_name =
      optimization_guide::ConvertBaseModelToUmaModelName(model);
  if (model_name != "Unknown") {
    ScopedDictPrefUpdate update(&local_state,
                                optimization_guide::model_execution::prefs::
                                    localstate::kOnDeviceAiPageActionState);
    update->Set(model_name, static_cast<int>(state));
  }
}

// Returns true if an earlier foundational model is installed or obsolete.
bool HasReplacedFoundationalModel(const OnDeviceModelControl& model_control) {
  using ModelInstallState = OnDeviceModelControl::ModelInstallState;
  for (OnDeviceBaseModel model :
       {OnDeviceBaseModel::kV3NanoCpu, OnDeviceBaseModel::kV3NanoGpu}) {
    const ModelInstallState state = model_control.GetModelInstallState(model);
    if (state == ModelInstallState::kInstalled ||
        state == ModelInstallState::kUninstalledObsolete) {
      return true;
    }
  }
  return false;
}

// Opens the on-device AI settings subpage in `tab`'s browser window.
void OnSettingsClicked(base::WeakPtr<tabs::TabInterface> tab) {
  if (tab && tab->GetBrowserWindowInterface()) {
    chrome::ShowSettingsSubPage(
        tab->GetBrowserWindowInterface(),
        base::FeatureList::IsEnabled(::features::kOnDeviceModelAdvancedSettings)
            ? chrome::kOnDeviceAiSubpage
            : chrome::kExperimentalAISettingsSubPage);
  }
}

}  // namespace

DEFINE_USER_DATA(AIPageActionController);

AIPageActionController::AIPageActionController(BrowserWindowInterface& browser)
    : browser_(browser),
      local_state_(g_browser_process ? g_browser_process->local_state()
                                     : nullptr),
      model_control_(GetModelControl()),
      scoped_unowned_user_data_(browser.GetUnownedUserDataHost(), *this) {
  if (model_control_) {
    observation_.Observe(model_control_);
  }
}

AIPageActionController::~AIPageActionController() = default;

// static
AIPageActionController* AIPageActionController::From(
    BrowserWindowInterface* browser) {
  return browser ? Get(browser->GetUnownedUserDataHost()) : nullptr;
}

void AIPageActionController::OnModelInstalled(OnDeviceBaseModel base_model) {
  if (local_state_ && base_model == OnDeviceBaseModel::kGemma4 &&
      GetPageActionState(*local_state_, base_model) ==
          PageActionState::kNotEligible) {
    SetPageActionState(*local_state_, base_model, PageActionState::kPending);
  }
}

void AIPageActionController::OnModelUninstalled(OnDeviceBaseModel base_model) {
  if (local_state_ && base_model == OnDeviceBaseModel::kGemma4 &&
      GetPageActionState(*local_state_, base_model) ==
          PageActionState::kPending) {
    SetPageActionState(*local_state_, base_model,
                       PageActionState::kNotEligible);
  }
}

void AIPageActionController::OnModelSessionCreated(OnDeviceBaseModel base_model,
                                                   tabs::TabInterface* tab) {
  if (base_model != OnDeviceBaseModel::kGemma4 || !local_state_ ||
      !model_control_ || !tab ||
      GetPageActionState(*local_state_, base_model) !=
          PageActionState::kPending) {
    return;
  }

  auto* const page_action_controller =
      page_actions::PageActionController::From(tab);
  if (!page_action_controller ||
      !page_action_controller->ActionExists(kActionOnDeviceAi)) {
    return;
  }

  SetPageActionState(*local_state_, base_model, PageActionState::kShown);

  const bool is_updated = HasReplacedFoundationalModel(*model_control_);
  const std::u16string chip_label =
      l10n_util::GetStringUTF16(is_updated ? IDS_ON_DEVICE_AI_UPDATED_CHIP_LABEL
                                           : IDS_ON_DEVICE_AI_READY_TITLE);
  page_action_controller->OverrideText(kActionOnDeviceAi, chip_label);
  page_action_controller->OverrideTooltip(kActionOnDeviceAi, chip_label);
  page_action_controller->Show(kActionOnDeviceAi);
  page_action_controller->ShowSuggestionChip(kActionOnDeviceAi,
                                             {.should_announce_chip = true});
  // TODO(crbug.com/585269149): Defer chip timer until `tab` is active.
  constexpr base::TimeDelta kChipVisibleDuration = base::Seconds(12);
  chip_collapse_timer_.Start(
      FROM_HERE, kChipVisibleDuration,
      base::BindOnce(
          [](base::WeakPtr<tabs::TabInterface> tab) {
            if (auto* controller =
                    tab ? page_actions::PageActionController::From(tab.get())
                        : nullptr) {
              controller->HideSuggestionChip(kActionOnDeviceAi);
            }
          },
          tab->GetWeakPtr()));
}

void AIPageActionController::ShowBubble(actions::ActionItem* action_item) {
  if (!model_control_ || !action_item || action_item->GetIsShowingBubble()) {
    return;
  }
  BrowserElements* const browser_elements = BrowserElements::From(&*browser_);
  if (!browser_elements) {
    return;
  }
  tabs::TabInterface* const tab = browser_->GetActiveTabInterface();
  auto* const page_action_controller =
      tab ? page_actions::PageActionController::From(tab) : nullptr;
  if (!page_action_controller ||
      !page_action_controller->ActionExists(kActionOnDeviceAi)) {
    return;
  }

  chip_collapse_timer_.Stop();
  const bool is_updated = HasReplacedFoundationalModel(*model_control_);
  const std::u16string title = l10n_util::GetStringUTF16(
      is_updated ? IDS_ON_DEVICE_AI_UPDATED_BUBBLE_TITLE
                 : IDS_ON_DEVICE_AI_READY_TITLE);
  action_item->SetIsShowingBubble(true);
  page_actions::ScopedPageActionActivity activity =
      page_action_controller->AddActivity(kActionOnDeviceAi);

  ui::DialogModel::Builder dialog_builder;
  dialog_builder.SetInternalName("OnDeviceAiBubble")
      .SetTitle(title)
      .AddParagraph(
          ui::DialogModelLabel(is_updated ? IDS_ON_DEVICE_AI_UPDATED_BUBBLE_BODY
                                          : IDS_ON_DEVICE_AI_ADDED_BUBBLE_BODY),
          std::u16string(), kOnDeviceAiBubbleElementId)
      .AddCancelButton(base::BindOnce(&OnSettingsClicked, tab->GetWeakPtr()),
                       ui::DialogModel::Button::Params().SetLabel(
                           l10n_util::GetStringUTF16(IDS_SETTINGS_SETTINGS)))
      .AddOkButton(base::DoNothing(),
                   ui::DialogModel::Button::Params().SetLabel(
                       l10n_util::GetStringUTF16(IDS_SETTINGS_GOT_IT)))
      .SetDialogDestroyingCallback(base::BindOnce(
          [](base::WeakPtr<tabs::TabInterface> tab,
             base::WeakPtr<actions::ActionItem> item,
             page_actions::ScopedPageActionActivity activity) {
            if (item) {
              item->SetIsShowingBubble(false);
            }
            if (auto* controller =
                    tab ? page_actions::PageActionController::From(tab.get())
                        : nullptr) {
              controller->HideSuggestionChip(kActionOnDeviceAi);
            }
          },
          tab->GetWeakPtr(), action_item->GetAsWeakPtr(), std::move(activity)));
  if (!is_updated) {
    auto feature_row =
        views::Builder<views::BoxLayoutView>()
            .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
            .SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kStart)
            .SetBetweenChildSpacing(
                views::LayoutProvider::Get()->GetDistanceMetric(
                    views::DISTANCE_RELATED_LABEL_HORIZONTAL))
            .AddChildren(
                views::Builder<views::ImageView>().SetImage(
                    ui::ImageModel::FromVectorIcon(vector_icons::kPenSparkIcon,
                                                   ui::kColorIcon)),
                views::Builder<views::Label>()
                    .SetText(l10n_util::GetStringUTF16(
                        IDS_ON_DEVICE_AI_FEATURE_GENERAL_PURPOSE_WRITING))
                    .SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT)
                    .SetTextStyle(views::style::STYLE_BODY_3_MEDIUM)
                    .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                    .SetMultiLine(true)
                    .SetProperty(views::kBoxLayoutFlexKey,
                                 views::BoxLayoutFlexSpecification()))
            .Build();
    dialog_builder.AddCustomField(
        std::make_unique<views::BubbleDialogModelHost::CustomView>(
            std::move(feature_row),
            views::BubbleDialogModelHost::FieldType::kText));
  }

  chrome::ShowBubble(browser_elements->GetContext(),
                     kPageActionOnDeviceAiElementId, dialog_builder.Build());
}
