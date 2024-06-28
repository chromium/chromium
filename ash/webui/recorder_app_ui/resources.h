// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_RECORDER_APP_UI_RESOURCES_H_
#define ASH_WEBUI_RECORDER_APP_UI_RESOURCES_H_

#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/base/webui/web_ui_util.h"

namespace ash {

const webui::LocalizedString kLocalizedStrings[] = {
    {"genAiDisclaimerText", IDS_RECORDER_GEN_AI_DISCLAIMER_TEXT},
    {"genAiExperimentBadge", IDS_RECORDER_GEN_AI_EXPERIMENT_BADGE},
    {"genAiLearnMoreLink", IDS_RECORDER_GEN_AI_LEARN_MORE_LINK},
    {"onboardingDialogSpeakerIdAllowButton",
     IDS_RECORDER_ONBOARDING_DIALOG_SPEAKER_ID_ALLOW_BUTTON},
    {"onboardingDialogSpeakerIdDescription",
     IDS_RECORDER_ONBOARDING_DIALOG_SPEAKER_ID_DESCRIPTION},
    {"onboardingDialogSpeakerIdDisallowButton",
     IDS_RECORDER_ONBOARDING_DIALOG_SPEAKER_ID_DISALLOW_BUTTON},
    {"onboardingDialogSpeakerIdHeader",
     IDS_RECORDER_ONBOARDING_DIALOG_SPEAKER_ID_HEADER},
    {"onboardingDialogTranscriptionCancelButton",
     IDS_RECORDER_ONBOARDING_DIALOG_TRANSCRIPTION_CANCEL_BUTTON},
    {"onboardingDialogTranscriptionDescription",
     IDS_RECORDER_ONBOARDING_DIALOG_TRANSCRIPTION_DESCRIPTION},
    {"onboardingDialogTranscriptionHeader",
     IDS_RECORDER_ONBOARDING_DIALOG_TRANSCRIPTION_HEADER},
    {"onboardingDialogTranscriptionTurnOnButton",
     IDS_RECORDER_ONBOARDING_DIALOG_TRANSCRIPTION_TURN_ON_BUTTON},
    {"onboardingDialogWelcomeDescription",
     IDS_RECORDER_ONBOARDING_DIALOG_WELCOME_DESCRIPTION},
    {"onboardingDialogWelcomeHeader",
     IDS_RECORDER_ONBOARDING_DIALOG_WELCOME_HEADER},
    {"onboardingDialogWelcomeNextButton",
     IDS_RECORDER_ONBOARDING_DIALOG_WELCOME_NEXT_BUTTON},
    {"recordDeleteDialogCancelButton",
     IDS_RECORDER_RECORD_DELETE_DIALOG_CANCEL_BUTTON},
    {"recordDeleteDialogDeleteButton",
     IDS_RECORDER_RECORD_DELETE_DIALOG_DELETE_BUTTON},
    {"recordDeleteDialogHeader", IDS_RECORDER_RECORD_DELETE_DIALOG_HEADER},
    {"recordExitDialogCancelButton",
     IDS_RECORDER_RECORD_EXIT_DIALOG_CANCEL_BUTTON},
    {"recordExitDialogDeleteButton",
     IDS_RECORDER_RECORD_EXIT_DIALOG_DELETE_BUTTON},
    {"recordExitDialogDescription",
     IDS_RECORDER_RECORD_EXIT_DIALOG_DESCRIPTION},
    {"recordExitDialogHeader", IDS_RECORDER_RECORD_EXIT_DIALOG_HEADER},
    {"recordExitDialogSaveAndExitButton",
     IDS_RECORDER_RECORD_EXIT_DIALOG_SAVE_AND_EXIT_BUTTON},
    {"recordStopButton", IDS_RECORDER_RECORD_STOP_BUTTON},
    {"recordingListHeader", IDS_RECORDER_RECORDING_LIST_HEADER},
    {"recordingListSortByDateOption",
     IDS_RECORDER_RECORDING_LIST_SORT_BY_DATE_OPTION},
    {"recordingListSortByNameOption",
     IDS_RECORDER_RECORDING_LIST_SORT_BY_NAME_OPTION},
    {"recordingListThisMonthHeader",
     IDS_RECORDER_RECORDING_LIST_THIS_MONTH_HEADER},
    {"recordingListTodayHeader", IDS_RECORDER_RECORDING_LIST_TODAY_HEADER},
    {"recordingListYesterdayHeader",
     IDS_RECORDER_RECORDING_LIST_YESTERDAY_HEADER},
    {"settingsHeader", IDS_RECORDER_SETTINGS_HEADER},
    {"settingsOptionsDoNotDisturbDescription",
     IDS_RECORDER_SETTINGS_OPTIONS_DO_NOT_DISTURB_DESCRIPTION},
    {"settingsOptionsDoNotDisturbLabel",
     IDS_RECORDER_SETTINGS_OPTIONS_DO_NOT_DISTURB_LABEL},
    {"settingsOptionsKeepScreenOnLabel",
     IDS_RECORDER_SETTINGS_OPTIONS_KEEP_SCREEN_ON_LABEL},
    {"settingsOptionsSpeakerIdDescription",
     IDS_RECORDER_SETTINGS_OPTIONS_SPEAKER_ID_DESCRIPTION},
    {"settingsOptionsSpeakerIdLabel",
     IDS_RECORDER_SETTINGS_OPTIONS_SPEAKER_ID_LABEL},
    {"settingsOptionsTranscriptionLabel",
     IDS_RECORDER_SETTINGS_OPTIONS_TRANSCRIPTION_LABEL},
    {"settingsSectionGeneralHeader",
     IDS_RECORDER_SETTINGS_SECTION_GENERAL_HEADER},
    {"settingsSectionTranscriptionSummaryHeader",
     IDS_RECORDER_SETTINGS_SECTION_TRANSCRIPTION_SUMMARY_HEADER},
    {"summaryHeader", IDS_RECORDER_SUMMARY_HEADER},
    {"titleGenerationHeader", IDS_RECORDER_TITLE_GENERATION_HEADER},
    {"titleRenameTooltip", IDS_RECORDER_TITLE_RENAME_TOOLTIP},
    {"transcriptionAutoscrollButton",
     IDS_RECORDER_TRANSCRIPTION_AUTOSCROLL_BUTTON},
};

}  // namespace ash

#endif  // ASH_WEBUI_RECORDER_APP_UI_RESOURCES_H_
