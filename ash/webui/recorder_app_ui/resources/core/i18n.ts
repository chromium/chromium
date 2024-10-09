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
  'exportDialogAudioFormatWebmOption',
  'exportDialogAudioHeader',
  'exportDialogCancelButton',
  'exportDialogHeader',
  'exportDialogSaveButton',
  'exportDialogTranscriptionFormatTxtOption',
  'exportDialogTranscriptionHeader',
  'genAiDisclaimerText',
  'genAiErrorGeneralLabel',
  'genAiErrorSummaryTrustAndSafetyLabel',
  'genAiErrorTitleSuggestionTrustAndSafetyLabel',
  'genAiExperimentBadge',
  'genAiLearnMoreLink',
  'genAiLearnMoreLinkTooltip',
  'genaiNegativeFeedbackButtonTooltip',
  'genaiPositiveFeedbackButtonTooltip',
  'mainRecordingBarLandmarkAriaLabel',
  'mainRecordingsListLandmarkAriaLabel',
  'mainSearchLandmarkAriaLabel',
  'mainStartRecordButtonTooltip',
  'mainStartRecordNudge',
  'micSelectionMenuButtonTooltip',
  'micSelectionMenuChromebookAudioOption',
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
  'onboardingDialogTranscriptionHeader',
  'onboardingDialogTranscriptionTurnOnButton',
  'onboardingDialogWelcomeDescription',
  'onboardingDialogWelcomeHeader',
  'onboardingDialogWelcomeNextButton',
  'playbackBackwardButtonTooltip',
  'playbackControlsLandmarkAriaLabel',
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
  'playbackVolumeAriaLabel',
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
  'recordInfoDialogDateLabel',
  'recordInfoDialogDurationLabel',
  'recordInfoDialogHeader',
  'recordInfoDialogSizeLabel',
  'recordInfoDialogTitleLabel',
  'recordMenuButtonTooltip',
  'recordMenuDeleteOption',
  'recordMenuToggleTranscriptionOption',
  'recordMuteButtonTooltip',
  'recordPauseButtonTooltip',
  'recordStopButton',
  'recordTranscriptButtonTooltip',
  'recordTranscriptionEntryPointDescription',
  'recordTranscriptionEntryPointDisableButton',
  'recordTranscriptionEntryPointEnableButton',
  'recordTranscriptionEntryPointHeader',
  'recordTranscriptionOffDescription',
  'recordTranscriptionOffHeader',
  'recordingItemOptionsButtonTooltip',
  'recordingItemPlayButtonTooltip',
  'recordingListHeader',
  'recordingListNoMatchText',
  'recordingListSearchBoxClearButtonAriaLabel',
  'recordingListSearchBoxCloseButtonAriaLabel',
  'recordingListSearchBoxPlaceholder',
  'recordingListSearchButtonTooltip',
  'recordingListSortButtonTooltip',
  'recordingListSortByDateOption',
  'recordingListSortByNameOption',
  'recordingListThisMonthHeader',
  'recordingListTodayHeader',
  'recordingListYesterdayHeader',
  'settingsHeader',
  'settingsOptionsDoNotDisturbDescription',
  'settingsOptionsDoNotDisturbLabel',
  'settingsOptionsKeepScreenOnLabel',
  'settingsOptionsSpeakerLabelDescription',
  'settingsOptionsSpeakerLabelLabel',
  'settingsOptionsSummaryDescription',
  'settingsOptionsSummaryDownloadButton',
  'settingsOptionsSummaryDownloadingButton',
  'settingsOptionsSummaryLabel',
  'settingsOptionsSummaryLearnMoreLink',
  'settingsOptionsTranscriptionDownloadButton',
  'settingsOptionsTranscriptionDownloadingButton',
  'settingsOptionsTranscriptionLabel',
  'settingsSectionGeneralHeader',
  'settingsSectionTranscriptionSummaryHeader',
  'summaryDisabledLabel',
  'summaryDownloadFinishedStatusMessage',
  'summaryDownloadModelDescription',
  'summaryDownloadModelDisableButton',
  'summaryDownloadModelDownloadButton',
  'summaryDownloadModelHeader',
  'summaryDownloadStartedStatusMessage',
  'summaryFailedStatusMessage',
  'summaryFinishedStatusMessage',
  'summaryHeader',
  'summaryStartedStatusMessage',
  'systemAudioConsentDialogCancelButton',
  'systemAudioConsentDialogConsentButton',
  'systemAudioConsentDialogDescription',
  'systemAudioConsentDialogHeader',
  'titleRenameSnackbarMessage',
  'titleRenameTooltip',
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
  genAiErrorSummaryTranscriptTooLongLabel: withArgs<[number]>(),
  genAiErrorSummaryTranscriptTooShortLabel: withArgs<[number]>(),
  genAiErrorTitleSuggestionTranscriptTooLongLabel: withArgs<[number]>(),
  genAiErrorTitleSuggestionTranscriptTooShortLabel: withArgs<[number]>(),
  recordingItemOptionsButtonAriaLabel: withArgs<[string]>(),
  recordingItemPauseButtonAriaLabel: withArgs<[string]>(),
  recordingItemPlayButtonAriaLabel: withArgs<[string]>(),
  settingsOptionsSummaryDownloadingProgressDescription: withArgs<[number]>(),
  settingsOptionsTranscriptionDownloadingProgressDescription:
    withArgs<[number]>(),
  summaryDownloadingProgressDescription: withArgs<[number]>(),
  transcriptionSpeakerLabelLabel: withArgs<[string]>(),
} satisfies Record<string, I18nArgType[]>;
type WithArgsStringNames = typeof withArgsStringNames;

function getI18nString(name: string): string {
  return PlatformHandler.getStringF(name);
}

function createI18nStringFormatter(name: string) {
  return (...args: I18nArgType[]) => {
    return PlatformHandler.getStringF(name, ...args);
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
