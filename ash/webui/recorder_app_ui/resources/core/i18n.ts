// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {usePlatformHandler} from './lit/context.js';
import {forceCast, upcast} from './utils/type_utils.js';

type I18nArgType = number|string;

/**
 * Helper for defining a localized string with arguments.
 *
 * This is only expected to be used in this file, and is currently exported
 * only to avoid unused function error.
 * TODO(pihsun): Remove "export" when there's user.
 */
export function withArgs<Args extends I18nArgType[]>(): Args {
  // This is only used as a type level info.
  return forceCast<Args>(null);
}

const noArgStrings = [
  'genAiDisclaimerText',
  'genAiExperimentBadge',
  'genAiLearnMoreLink',
  'onboardingDialogSpeakerIdAllowButton',
  'onboardingDialogSpeakerIdDescription',
  'onboardingDialogSpeakerIdDisallowButton',
  'onboardingDialogSpeakerIdHeader',
  'onboardingDialogTranscriptionCancelButton',
  'onboardingDialogTranscriptionDescription',
  'onboardingDialogTranscriptionHeader',
  'onboardingDialogTranscriptionTurnOnButton',
  'onboardingDialogWelcomeDescription',
  'onboardingDialogWelcomeHeader',
  'onboardingDialogWelcomeNextButton',
  'recordDeleteDialogCancelButton',
  'recordDeleteDialogDeleteButton',
  'recordDeleteDialogHeader',
  'recordExitDialogCancelButton',
  'recordExitDialogDeleteButton',
  'recordExitDialogDescription',
  'recordExitDialogHeader',
  'recordExitDialogSaveAndExitButton',
  'recordStopButton',
  'recordingListHeader',
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
  'settingsOptionsTranscriptionLabel',
  'settingsSectionGeneralHeader',
  'settingsSectionTranscriptionSummaryHeader',
  'summaryHeader',
  'titleGenerationHeader',
  'titleRenameTooltip',
  'transcriptionAutoscrollButton',
] as const;

type NoArgStrings = (typeof noArgStrings)[number];

const withArgsStrings = {
  // This contains all the other strings that needs argument.
  // Usage example:
  // Add `fooBar: withArgs<[number, string]>(),` here,
  // then `i18n.fooBar(1, '2')` works.
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
