// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/common/sea_pen_resources.h"

#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/base/webui/web_ui_util.h"

namespace ash::common {

void AddSeaPenStrings(content::WebUIDataSource* source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"seaPenExclusiveWallpapersHeading",
       IDS_PERSONALIZATION_APP_EXCLUSIVE_WALLPAPERS_HEADING},
      {"seaPenChooseAWallpaperHeading",
       IDS_PERSONALIZATION_APP_CHOOSE_A_WALLPAPER_HEADING},
      {"seaPenLabel", IDS_SEA_PEN_LABEL},
      {"vcBackgroundLabel", IDS_VC_BACKGROUND_LABEL},
      {"seaPenPoweredByGoogleAi", IDS_SEA_PEN_POWERED_BY_GOOGLE_AI},
      {"seaPenTemplateHeading", IDS_SEA_PEN_TEMPLATE_HEADING},
      {"seaPenRecentWallpapersHeading", IDS_SEA_PEN_RECENT_WALLPAPERS_HEADING},
      {"vcBackgroundRecentWallpapersHeading",
       IDS_VC_BACKGROUND_RECENT_WALLPAPERS_HEADING},
      {"seaPenInspireMeButton", IDS_SEA_PEN_INSPIRE_ME_BUTTON},
      {"seaPenCreateButton", IDS_SEA_PEN_CREATE_BUTTON},
      {"seaPenRecreateButton", IDS_SEA_PEN_RECREATE_BUTTON},
      {"seaPenWallpaperPoweredByGoogle",
       IDS_SEA_PEN_WALLPAPER_POWERED_BY_GOOGLE},
      {"vcBackgroundPoweredByGoogle", IDS_VC_BACKGROUND_POWERED_BY_GOOGLE},
      {"seaPenDeleteWallpaper", IDS_SEA_PEN_DELETE_WALLPAPER},
      {"seaPenCreateMore", IDS_SEA_PEN_CREATE_MORE},
      {"seaPenAbout", IDS_SEA_PEN_ABOUT},
      {"seaPenAboutDialogTitle", IDS_SEA_PEN_ABOUT_DIALOG_TITLE},
      {"vcBackgroundAboutDialogTitle", IDS_VC_BACKGROUND_ABOUT_DIALOG_TITLE},
      {"seaPenAboutDialogPrompt", IDS_SEA_PEN_ABOUT_DIALOG_PROMPT},
      {"vcBackgroundAboutDialogPrompt", IDS_VC_BACKGROUND_ABOUT_DIALOG_PROMPT},
      {"seaPenAboutDialogDate", IDS_SEA_PEN_ABOUT_DIALOG_DATE},
      {"seaPenAboutDialogClose", IDS_SEA_PEN_ABOUT_DIALOG_CLOSE},
      {"seaPenErrorNoInternet", IDS_SEA_PEN_ERROR_NO_INTERNET},
      {"seaPenErrorResourceExhausted", IDS_SEA_PEN_ERROR_RESOURCE_EXHAUSTED},
      {"seaPenErrorGeneric", IDS_SEA_PEN_ERROR_GENERIC},
      {"seaPenExperimentLabel", IDS_SEA_PEN_EXPERIMENT_LABEL},
      {"seaPenUnavailableLabel", IDS_SEA_PEN_UNAVAILABLE_LABEL},
      {"seaPenThumbnailsLoading", IDS_SEA_PEN_THUMBNAILS_LOADING},
      {"seaPenCreatingHighResImage", IDS_SEA_PEN_CREATING_HIGH_RES_IMAGE},
      {"seaPenExpandOptionsButton", IDS_SEA_PEN_EXPAND_OPTIONS_BUTTON},
      {"seaPenRecentImageMenuButton", IDS_SEA_PEN_RECENT_IMAGE_MENU_BUTTON},
      {"seaPenMenuRoleDescription", IDS_SEA_PEN_MENU_ROLE_DESCRIPTION},
      {"seaPenCustomizeAiPrompt", IDS_SEA_PEN_CUSTOMIZE_AI_PROMPT},
      {"seaPenFeedbackDescription", IDS_SEA_PEN_FEEDBACK_DESCRIPTION},
      {"seaPenFeedbackPositive", IDS_SEA_PEN_FEEDBACK_POSITIVE},
      {"seaPenFeedbackNegative", IDS_SEA_PEN_FEEDBACK_NEGATIVE},
      {"seaPenSetWallpaper", IDS_SEA_PEN_SET_WALLPAPER},
      {"seaPenSetCameraBackground", IDS_SEA_PEN_SET_CAMERA_BACKGROUND},
      {"seaPenLabel", IDS_SEA_PEN_LABEL},
      {"seaPenZeroStateMessage", IDS_SEA_PEN_ZERO_STATE_MESSAGE},
      {"seaPenIntroductionTitle", IDS_SEA_PEN_INTRODUCTION_DIALOG_TITLE},
      {"seaPenIntroductionContent", IDS_SEA_PEN_INTRODUCTION_DIALOG_CONTENT},
      {"seaPenIntroductionDialogCloseButton",
       IDS_SEA_PEN_INTRODUCTION_DIALOG_CLOSE_BUTTON},
      {"seaPenFreeformWallpaperTemplatesLabel",
       IDS_SEA_PEN_FREEFORM_WALLPAPER_TEMPLATES_LABEL},
      {"seaPenFreeformSamplePromptsLabel",
       IDS_SEA_PEN_FREEFORM_SAMPLE_PROMPTS_LABEL},
      {"seaPenFreeformResultsLabel", IDS_SEA_PEN_FREEFORM_RESULTS_LABEL},
      {"seaPenFreeformShuffle", IDS_SEA_PEN_FREEFORM_SHUFFLE},
      {"seaPenFreeformInputPlaceholder",
       IDS_SEA_PEN_FREEFORM_INPUT_PLACEHOLDER},
      {"seaPenFreeformErrorNoInternet", IDS_SEA_PEN_FREEFORM_ERROR_NO_INTERNET},
      {"seaPenFreeformErrorUnsupportedLanguage",
       IDS_SEA_PEN_FREEFORM_ERROR_UNSUPPORTED_LANGUAGE},
      {"seaPenFreeformErrorBlockedOutputs",
       IDS_SEA_PEN_FREEFORM_ERROR_BLOCKED_OUTPUTS},
      {"seaPenFreeformPoweredByGoogle", IDS_SEA_PEN_FREEFORM_POWERED_BY_GOOGLE},
      {"seaPenFreeformPreviousPrompts", IDS_SEA_PEN_FREEFORM_PREVIOUS_PROMPTS},
      {"seaPenFreeformPreviousPromptsTooltip",
       IDS_SEA_PEN_FREEFORM_PREVIOUS_PROMPTS_TOOLTIP},
      {"seaPenFreeformMoreSuggestionsButton",
       IDS_SEA_PEN_FREEFORM_MORE_SUGGESTIONS_BUTTON},
      {"seaPenFreeformPromptingGuide", IDS_SEA_PEN_FREEFORM_PROMPTING_GUIDE},
      {"seaPenFreeformNavigationDescription",
       IDS_SEA_PEN_FREEFORM_NAVIGATION_DESCRIPTION},
      {"seaPenIntroductionDialogFirstParagraph",
       IDS_SEA_PEN_INTRODUCTION_DIALOG_FIRST_PARAGRAPH},
      {"seaPenFreeformIntroductionDialogFirstParagraph",
       IDS_SEA_PEN_FREEFORM_INTRODUCTION_DIALOG_FIRST_PARAGRAPH},

      {"seaPenDismissError", IDS_PERSONALIZATION_APP_DISMISS},
      {"ariaLabelLoading", IDS_PERSONALIZATION_APP_ARIA_LABEL_LOADING},
      {"seaPenManagedLabel", IDS_ASH_ENTERPRISE_DEVICE_MANAGED_SHORT},
  };
  source->AddLocalizedStrings(kLocalizedStrings);
}

}  // namespace ash::common
