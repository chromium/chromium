// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {join} from 'chrome://resources/mwc/lit/index.js';

import {PlatformHandler} from '../platforms/index.js';

import {forceCast} from './utils/type_utils.js';

type I18nArgType = number|string;

/**
 * Helper for defining a localized string with arguments.
 */
function withArgs<Args extends I18nArgType[]>(): Args {
  // This is only used as a type level info.
  return forceCast<Args>(null);
}

const noArgStringNames = [
  'appName',
  'backToMainButtonAriaLabel',
  'backToMainButtonTooltip',
  'closeDialogButtonTooltip',
  'errorDialogConsentButton',
  'exportDialogAudioFormatWebmOption',
  'exportDialogAudioHeader',
  'exportDialogCancelButton',
  'exportDialogHeader',
  'exportDialogSaveButton',
  'exportDialogTranscriptionFormatTxtOption',
  'exportDialogTranscriptionHeader',
  'genAiDisclaimerText',
  'genAiDownloadErrorStatusMessage',
  'genAiDownloadFinishedStatusMessage',
  'genAiDownloadStartedStatusMessage',
  'genAiErrorGeneralLabel',
  'genAiErrorModelDownloadButton',
  'genAiErrorModelDownloadButtonAriaLabel',
  'genAiErrorModelLoadFailureLabel',
  'genAiErrorSummaryLanguageUnsupportedLabel',
  'genAiErrorSummaryNeedsRebootLabel',
  'genAiErrorSummaryTranscriptTooLongLabel',
  'genAiErrorSummaryTranscriptTooShortLabel',
  'genAiErrorSummaryTrustAndSafetyLabel',
  'genAiErrorTitleSuggestionLanguageUnsupportedLabel',
  'genAiErrorTitleSuggestionNeedsRebootLabel',
  'genAiErrorTitleSuggestionTranscriptTooLongLabel',
  'genAiErrorTitleSuggestionTranscriptTooShortLabel',
  'genAiErrorTitleSuggestionTrustAndSafetyLabel',
  'genAiExperimentBadge',
  'genAiFeedbackModelInputField',
  'genAiFeedbackPrompt',
  'genAiFeedbackSummaryOutputField',
  'genAiFeedbackTitleSuggestionOutputField',
  'genAiLearnMoreLink',
  'genAiLearnMoreLinkTooltip',
  'genAiNeedsRebootStatusMessage',
  'genaiNegativeFeedbackButtonTooltip',
  'genaiPositiveFeedbackButtonTooltip',
  'languageDropdownHintOption',
  'languagePickerAvailableLanguagesHeader',
  'languagePickerBackButtonAriaLabel',
  'languagePickerBackButtonTooltip',
  'languagePickerHeader',
  'languagePickerLanguageDownloadButton',
  'languagePickerLanguageDownloadingButton',
  'languagePickerLanguageErrorDescription',
  'languagePickerLanguageNeedsRebootDescription',
  'languagePickerLanguagesListLandmarkAriaLabel',
  'languagePickerSelectedLanguageHeader',
  'languagePickerSelectedLanguageNoneLabel',
  'mainChooseMicNudge',
  'mainRecordingBarLandmarkAriaLabel',
  'mainRecordingsListLandmarkAriaLabel',
  'mainSearchLandmarkAriaLabel',
  'mainStartRecordButtonTooltip',
  'mainStartRecordNudge',
  'micConnectionErrorDialogDescription',
  'micConnectionErrorDialogHeader',
  'micSelectionMenuButtonTooltip',
  'micSelectionMenuMicConnectionErrorDescription',
  'micSelectionMenuSystemAudioOption',
  'onboardingDialogLanguageSelectionCancelButton',
  'onboardingDialogLanguageSelectionDescription',
  'onboardingDialogLanguageSelectionDownloadButton',
  'onboardingDialogLanguageSelectionHeader',
  'onboardingDialogSpeakerLabelAllowButton',
  'onboardingDialogSpeakerLabelDeferButton',
  'onboardingDialogSpeakerLabelDescriptionListItem1',
  'onboardingDialogSpeakerLabelDescriptionListItem2',
  'onboardingDialogSpeakerLabelDescriptionListItem3',
  'onboardingDialogSpeakerLabelDescriptionPrefix',
  'onboardingDialogSpeakerLabelDescriptionSuffix',
  'onboardingDialogSpeakerLabelDisallowButton',
  'onboardingDialogSpeakerLabelHeader',
  'onboardingDialogSpeakerLabelLearnMoreLink',
  'onboardingDialogTranscriptionCancelButton',
  'onboardingDialogTranscriptionDeferButton',
  'onboardingDialogTranscriptionDescription',
  'onboardingDialogTranscriptionDownloadButton',
  'onboardingDialogTranscriptionHeader',
  'onboardingDialogTranscriptionTurnOnButton',
  'onboardingDialogTranscriptionTurnOnDescription',
  'onboardingDialogTranscriptionTurnOnHeader',
  'onboardingDialogWelcomeDescription',
  'onboardingDialogWelcomeHeader',
  'onboardingDialogWelcomeNextButton',
  'playbackBackwardButtonTooltip',
  'playbackControlsLandmarkAriaLabel',
  'playbackFloatingVolumeCloseButtonAriaLabel',
  'playbackFloatingVolumeShowButtonAriaLabel',
  'playbackForwardButtonTooltip',
  'playbackHideTranscriptButtonTooltip',
  'playbackMenuButtonTooltip',
  'playbackMenuDeleteOption',
  'playbackMenuExportOption',
  'playbackMenuShowDetailOption',
  'playbackMuteButtonTooltip',
  'playbackPauseButtonTooltip',
  'playbackPlayButtonTooltip',
  'playbackSeekSliderAriaLabel',
  'playbackShowTranscriptButtonTooltip',
  'playbackSpeedButtonTooltip',
  'playbackSpeedNormalOption',
  'playbackTranscriptLandmarkAriaLabel',
  'playbackUnmuteButtonTooltip',
  'playbackVolumeSliderAriaLabel',
  'recordDeleteButtonTooltip',
  'recordDeleteDialogCancelButton',
  'recordDeleteDialogCurrentHeader',
  'recordDeleteDialogDeleteButton',
  'recordDeleteDialogDescription',
  'recordDeleteDialogHeader',
  'recordExitDialogCancelButton',
  'recordExitDialogDeleteButton',
  'recordExitDialogDescription',
  'recordExitDialogHeader',
  'recordExitDialogSaveAndExitButton',
  'recordGeneralAudioErrorDialogDescription',
  'recordGeneralAudioErrorDialogHeader',
  'recordHideTranscriptButtonTooltip',
  'recordInfoDialogDateLabel',
  'recordInfoDialogDurationLabel',
  'recordInfoDialogHeader',
  'recordInfoDialogSizeLabel',
  'recordInfoDialogTitleLabel',
  'recordMenuButtonTooltip',
  'recordMenuDeleteOption',
  'recordMenuToggleSpeakerLabelOption',
  'recordMenuToggleTranscriptionOption',
  'recordMuteButtonTooltip',
  'recordPauseButtonTooltip',
  'recordResumeButtonTooltip',
  'recordShowTranscriptButtonTooltip',
  'recordStopButton',
  'recordTranscriptionEntryPointDescription',
  'recordTranscriptionEntryPointDisableButton',
  'recordTranscriptionEntryPointEnableButton',
  'recordTranscriptionEntryPointHeader',
  'recordTranscriptionOffDescription',
  'recordTranscriptionOffHeader',
  'recordTranscriptionUnusableErrorDescription',
  'recordTranscriptionUnusableHeader',
  'recordTranscriptionUnusableNeedsRebootDescription',
  'recordTranscriptionUnusableNotInstalledDescription',
  'recordTranscriptionUnusableSelectLanguageDescription',
  'recordTranscriptionWaitingDownloadText',
  'recordUnmuteButtonTooltip',
  'recordingItemOptionsButtonTooltip',
  'recordingItemPauseButtonTooltip',
  'recordingItemPlayButtonTooltip',
  'recordingListHeader',
  'recordingListNoMatchText',
  'recordingListSearchBoxClearButtonAriaLabel',
  'recordingListSearchBoxCloseButtonAriaLabel',
  'recordingListSearchBoxPlaceholder',
  'recordingListSearchButtonTooltip',
  'recordingListSortButtonTooltip',
  'recordingListSortByDateOption',
  'recordingListSortByTitleOption',
  'recordingListThisMonthHeader',
  'recordingListTodayHeader',
  'recordingListYesterdayHeader',
  'settingsHeader',
  'settingsOptionsDoNotDisturbDescription',
  'settingsOptionsDoNotDisturbLabel',
  'settingsOptionsGenAiDescription',
  'settingsOptionsGenAiDownloadButton',
  'settingsOptionsGenAiDownloadButtonAriaLabel',
  'settingsOptionsGenAiDownloadingButton',
  'settingsOptionsGenAiDownloadingProgressDescription',
  'settingsOptionsGenAiErrorDescription',
  'settingsOptionsGenAiLabel',
  'settingsOptionsGenAiLearnMoreLink',
  'settingsOptionsGenAiLearnMoreLinkAriaLabel',
  'settingsOptionsGenAiNeedsRebootDescription',
  'settingsOptionsKeepScreenOnLabel',
  'settingsOptionsLanguageSubpageButtonAriaLabel',
  'settingsOptionsSpeakerLabelDescription',
  'settingsOptionsSpeakerLabelLabel',
  'settingsOptionsTranscriptionDownloadButton',
  'settingsOptionsTranscriptionDownloadButtonAriaLabel',
  'settingsOptionsTranscriptionDownloadingButton',
  'settingsOptionsTranscriptionErrorDescription',
  'settingsOptionsTranscriptionLabel',
  'settingsOptionsTranscriptionLanguageDescription',
  'settingsOptionsTranscriptionLanguageLabel',
  'settingsOptionsTranscriptionNeedsRebootDescription',
  'settingsSectionGeneralHeader',
  'settingsSectionTranscriptionSummaryHeader',
  'summaryCollapseTooltip',
  'summaryDisabledLabel',
  'summaryDownloadGenAiModelDescription',
  'summaryDownloadGenAiModelDisableButton',
  'summaryDownloadGenAiModelDownloadButton',
  'summaryDownloadGenAiModelHeader',
  'summaryExpandTooltip',
  'summaryFailedStatusMessage',
  'summaryFinishedStatusMessage',
  'summaryHeader',
  'summaryStartedStatusMessage',
  'systemAudioConsentDialogCancelButton',
  'systemAudioConsentDialogConsentButton',
  'systemAudioConsentDialogDescription',
  'systemAudioConsentDialogHeader',
  'titleEditSnackbarMessage',
  'titleEditTooltip',
  'titleSuggestionButtonTooltip',
  'titleSuggestionFailedStatusMessage',
  'titleSuggestionFinishedStatusMessage',
  'titleSuggestionHeader',
  'titleSuggestionStartedStatusMessage',
  'titleTextfieldAriaLabel',
  'transcriptionAutoscrollButton',
  'transcriptionNoSpeechText',
  'transcriptionSpeakerLabelPendingLabel',
  'transcriptionWaitingSpeechText',
] as const;

export type NoArgStringName = (typeof noArgStringNames)[number];

const withArgsStringNames = {
  // This contains all the other strings that needs argument.
  // Usage example:
  // Add `fooBar: withArgs<[number, string]>(),` here,
  // then `i18n.fooBar(1, '2')` works.
  languagePickerLanguageDownloadButtonAriaLabel: withArgs<[string]>(),
  languagePickerLanguageDownloadErrorAriaLabel: withArgs<[string]>(),
  languagePickerLanguageDownloadErrorStatusMessage: withArgs<[string]>(),
  languagePickerLanguageDownloadFinishedStatusMessage: withArgs<[string]>(),
  languagePickerLanguageDownloadStartedStatusMessage: withArgs<[string]>(),
  languagePickerLanguageDownloadingAriaLabel: withArgs<[string, number]>(),
  languagePickerLanguageDownloadingProgressDescription: withArgs<[number]>(),
  languagePickerLanguageNeedsRebootAriaLabel: withArgs<[string]>(),
  languagePickerLanguageNeedsRebootStatusMessage: withArgs<[string]>(),
  languagePickerLanguageNotDownloadedAriaLabel: withArgs<[string]>(),
  languagePickerLanguageNotSelectedAriaLabel: withArgs<[string]>(),
  languagePickerLanguageSelectedAriaLabel: withArgs<[string]>(),
  recordingItemOptionsButtonAriaLabel: withArgs<[string]>(),
  recordingItemPauseButtonAriaLabel: withArgs<[string]>(),
  recordingItemPlayButtonAriaLabel: withArgs<[string]>(),
  settingsOptionsGenAiDownloadingProgressDescription: withArgs<[number]>(),
  settingsOptionsTranscriptionDownloadingProgressDescription:
    withArgs<[number]>(),
  summaryGenAiDownloadingProgressDescription: withArgs<[number]>(),
  transcriptionSpeakerLabelLabel: withArgs<[string]>(),
} satisfies Record<string, I18nArgType[]>;
type WithArgsStringNames = typeof withArgsStringNames;

const withDeviceStringNames = [
  'genAiDownloadErrorStatusMessage',
  'genAiErrorModelLoadFailureLabel',
  'genAiErrorSummaryNeedsRebootLabel',
  'genAiErrorTitleSuggestionLanguageUnsupportedLabel',
  'genAiErrorTitleSuggestionNeedsRebootLabel',
  'genAiNeedsRebootStatusMessage',
  'languagePickerLanguageDownloadErrorAriaLabel',
  'languagePickerLanguageDownloadErrorStatusMessage',
  'languagePickerLanguageErrorDescription',
  'languagePickerLanguageNeedsRebootAriaLabel',
  'languagePickerLanguageNeedsRebootDescription',
  'languagePickerLanguageNeedsRebootStatusMessage',
  'micSelectionMenuSystemAudioOption',
  'recordGeneralAudioErrorDialogDescription',
  'recordTranscriptionUnusableErrorDescription',
  'recordTranscriptionUnusableNeedsRebootDescription',
  'settingsOptionsGenAiErrorDescription',
  'settingsOptionsGenAiNeedsRebootDescription',
  'settingsOptionsTranscriptionErrorDescription',
  'settingsOptionsTranscriptionNeedsRebootDescription',
  'systemAudioConsentDialogDescription',
  'systemAudioConsentDialogHeader',
];

function maybeReplaceDeviceType(name: string, i18nString: string) {
  if (withDeviceStringNames.includes(name)) {
    return i18nString.replaceAll(
      '[deviceType]',
      PlatformHandler.getDeviceType(),
    );
  }
  return i18nString;
}

function getI18nString(name: string): string {
  return maybeReplaceDeviceType(name, PlatformHandler.getStringF(name));
}

function createI18nStringFormatter(name: string) {
  return (...args: I18nArgType[]) => {
    return maybeReplaceDeviceType(
      name,
      PlatformHandler.getStringF(name, ...args),
    );
  };
}

type I18nObjectType = Record<NoArgStringName, string>&{
  [k in keyof WithArgsStringNames]: (...args: WithArgsStringNames[k]) => string;
};

/**
 * Entry point for accessing localized strings.
 *
 * @example
 *   i18n.foo  // For strings without arguments.
 *   i18n.bar('arg1', 2)  // For strings with arguments.
 */
// forceCast: TypeScript can't deduce type for Object.fromEntries correctly.
export const i18n = forceCast<I18nObjectType>(
  Object.fromEntries([
    ...noArgStringNames.map((name) => [name, getI18nString(name)] as const),
    ...Object.keys(withArgsStringNames)
      .map(
        (name) => [name, createI18nStringFormatter(name)] as const,
      ),
  ]),
);

/**
 * Replaces `placeholder` in a string with the given lit template.
 *
 * Note that this shouldn't be used when `html` is a simple string. In that
 * case, use the standard $0, $1 as an argument and `withArgsStrings` above.
 *
 * @param s The translated string.
 * @param placeholder The placeholder string to be replaced.
 * @param html The lit template to replace `placeholder` with.
 * @return The result template.
 */
export function replacePlaceholderWithHtml(
  s: string,
  placeholder: string,
  html: RenderResult,
): RenderResult {
  const parts = s.split(placeholder);
  if (parts.length <= 1) {
    // The placeholder should still exist after translation, so this is likely
    // an error in translation.
    console.error(
      `Translated string doesn't contain expected placeholder`,
      s,
      placeholder,
    );
  }
  return join(parts, () => html);
}
