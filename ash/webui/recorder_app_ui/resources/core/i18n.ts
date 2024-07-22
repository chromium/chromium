// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {join} from 'chrome://resources/mwc/lit/index.js';

import {usePlatformHandler} from './lit/context.js';
import {forceCast, upcast} from './utils/type_utils.js';

type I18nArgType = number|string;

/**
 * Helper for defining a localized string with arguments.
 */
function withArgs<Args extends I18nArgType[]>(): Args {
  // This is only used as a type level info.
  return forceCast<Args>(null);
}

const noArgStrings = [
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
  'micSelectionMenuChromebookAudioOption',
  'onboardingDialogSpeakerIdAllowButton',
  'onboardingDialogSpeakerIdDeferButton',
  'onboardingDialogSpeakerIdDescription',
  'onboardingDialogSpeakerIdDisallowButton',
  'onboardingDialogSpeakerIdHeader',
  'onboardingDialogSpeakerIdLearnMoreLink',
  'onboardingDialogTranscriptionCancelButton',
  'onboardingDialogTranscriptionDeferButton',
  'onboardingDialogTranscriptionDescription',
  'onboardingDialogTranscriptionHeader',
  'onboardingDialogTranscriptionTurnOnButton',
  'onboardingDialogWelcomeDescription',
  'onboardingDialogWelcomeHeader',
  'onboardingDialogWelcomeNextButton',
  'playbackMenuDeleteOption',
  'playbackMenuExportOption',
  'playbackMenuShowDetailOption',
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
  'recordMenuDeleteOption',
  'recordMenuToggleTranscriptionOption',
  'recordStopButton',
  'recordTranscriptionEntryPointDescription',
  'recordTranscriptionEntryPointDisableButton',
  'recordTranscriptionEntryPointEnableButton',
  'recordTranscriptionEntryPointHeader',
  'recordTranscriptionOffDescription',
  'recordTranscriptionOffHeader',
  'recordingListHeader',
  'recordingListNoMatchText',
  'recordingListSearchBoxPlaceholder',
  'recordingListSortByDateOption',
  'recordingListSortByNameOption',
  'recordingListThisMonthHeader',
  'recordingListTodayHeader',
  'recordingListYesterdayHeader',
  'settingsHeader',
  'settingsOptionsDoNotDisturbDescription',
  'settingsOptionsDoNotDisturbLabel',
  'settingsOptionsKeepScreenOnLabel',
  'settingsOptionsSpeakerIdDescription',
  'settingsOptionsSpeakerIdLabel',
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
  'summaryDownloadModelDescription',
  'summaryDownloadModelDisableButton',
  'summaryDownloadModelDownloadButton',
  'summaryDownloadModelHeader',
  'summaryHeader',
  'titleRenameTooltip',
  'titleSuggestionHeader',
  'transcriptionAutoscrollButton',
  'transcriptionNoSpeechText',
  'transcriptionWaitingSpeechText',
] as const;

type NoArgStrings = (typeof noArgStrings)[number];

const withArgsStrings = {
  // This contains all the other strings that needs argument.
  // Usage example:
  // Add `fooBar: withArgs<[number, string]>(),` here,
  // then `i18n.fooBar(1, '2')` works.
  settingsOptionsSummaryDownloadingProgressDescription: withArgs<[number]>(),
  settingsOptionsTranscriptionDownloadingProgressDescription:
    withArgs<[number]>(),
  summaryDownloadingProgressDescription: withArgs<[number]>(),
} satisfies Record<string, I18nArgType[]>;
type WithArgsStrings = typeof withArgsStrings;

type I18nType = Record<NoArgStrings, string>&{
  [k in keyof WithArgsStrings]: (...args: WithArgsStrings[k]) => string;
};

/**
 * Entry point for accessing localized strings.
 *
 * @example
 *   i18n.foo  // For strings without arguments.
 *   i18n.bar('arg1', 2)  // For strings with arguments.
 */
// TODO(pihsun): Have some initialize code to initialize i18n to a concrete
// object instead of having it as a proxy. Since it use usePlatformHandler()
// which are not available at module import time, we'll need to initialize it
// separately similar to other context states.
//
// forceCast: The proxy wrapper changed the type of the target.
export const i18n = forceCast<I18nType>(
  new Proxy(
    {},
    {
      get(_target, name) {
        if (typeof name !== 'string') {
          return;
        }
        if (upcast<readonly string[]>(noArgStrings).includes(name)) {
          return usePlatformHandler().getStringF(name);
        }
        if (Object.hasOwn(withArgsStrings, name)) {
          return (...args: I18nArgType[]) => {
            return usePlatformHandler().getStringF(name, ...args);
          };
        }
        return undefined;
      },
    },
  ),
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
