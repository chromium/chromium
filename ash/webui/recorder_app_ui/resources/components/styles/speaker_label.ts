// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {css} from 'chrome://resources/mwc/lit/index.js';

/**
 * Maximum number of the speaker colors before the color starts cycling.
 *
 * Note that the number of SPEECH_SPEAKER_COLOR_ entries in TimelineSegmentKind
 * in recording_data_manager.ts should be keep in sync with this.
 */
export const MAX_SPEAKER_COLORS = 5;

export const SPEAKER_LABEL_COLORS = css`
  /* Color tokens for different number of speakers. */
  .speaker-single,
  .speaker-duo,
  .speaker-multiple {
    /*
     * Default to error color for everything so it's clear if some token is
     * used in incorrect context. (Specifically, speaker label colors are not
     * defined when number of speaker is 0 or 1.)
     */
    --speaker-label-container-color: var(--cros-sys-error);
    --speaker-label-label-color: var(--cros-sys-error);
    --speaker-label-shapes-color: var(--cros-sys-error);
  }

  .speaker-duo {
    --speaker-label-label-color: var(--cros-sys-surface);

    & .speaker-1 {
      --speaker-label-container-color: var(--cros-sys-illo-card-color4);
      --speaker-label-shapes-color: var(--cros-sys-illo-card-on_color4);
    }

    & .speaker-2 {
      --speaker-label-container-color: var(--cros-sys-illo-card-color5);
      --speaker-label-shapes-color: var(--cros-sys-illo-card-on_color5);
    }
  }

  .speaker-multiple {
    --speaker-label-label-color: var(--cros-sys-surface);

    & .speaker-1 {
      --speaker-label-container-color: var(--cros-sys-illo-card-color4);
      --speaker-label-shapes-color: var(--cros-sys-illo-card-on_color4);
    }

    & .speaker-2 {
      --speaker-label-container-color: var(--cros-sys-illo-card-color5);
      --speaker-label-shapes-color: var(--cros-sys-illo-card-on_color5);
    }

    & .speaker-3 {
      --speaker-label-container-color: var(--cros-sys-illo-card-color1);
      --speaker-label-shapes-color: var(--cros-sys-illo-card-on_color1);
    }

    & .speaker-4 {
      --speaker-label-container-color: var(--cros-sys-illo-card-color2);
      --speaker-label-shapes-color: var(--cros-sys-illo-card-on_color2);
    }

    & .speaker-5 {
      --speaker-label-container-color: var(--cros-sys-illo-card-color3);
      --speaker-label-shapes-color: var(--cros-sys-illo-card-on_color3);
    }
  }
`;

/**
 * Gets the CSS class according to the total number of speaker labels.
 *
 * This should be applied to the containing element.
 */
export function getNumSpeakerClass(numSpeakers: number): string {
  if (numSpeakers <= 1) {
    return 'speaker-single';
  } else if (numSpeakers === 2) {
    return 'speaker-duo';
  } else {
    return 'speaker-multiple';
  }
}

/**
 * Getst the CSS class according to the 0-index speaker label.
 *
 * This should be applied to the element corresponds to a particular speaker
 * label.
 */
export function getSpeakerLabelClass(speakerLabelIndex: number): string {
  return `speaker-${(speakerLabelIndex % MAX_SPEAKER_COLORS) + 1}`;
}
