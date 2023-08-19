// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReached} from '../../assert.js';
import * as dom from '../../dom.js';
import {I18nString} from '../../i18n_string.js';
import * as loadTimeData from '../../models/load_time_data.js';
import {
  AspectRatioSet,
  Facing,
  PhotoResolutionLevel,
  VideoResolutionLevel,
} from '../../type.js';
import * as util from '../../util.js';

/**
 * Gets i18n label for camera facing.
 */
export function getLabelFromFacing(facing: Facing): I18nString {
  switch (facing) {
    case Facing.USER:
      return I18nString.LABEL_FRONT_CAMERA;
    case Facing.ENVIRONMENT:
      return I18nString.LABEL_BACK_CAMERA;
    case Facing.EXTERNAL:
      return I18nString.LABEL_EXTERNAL_CAMERA;
    default:
      assertNotReached();
  }
}

/**
 * Given a menu, add a text item with the given string to the menu using the
 * given template.
 *
 * @param menu The menu where the new text item added.
 * @param templateSelector The selector of the text item template.
 * @param i18nText The i18n string to be displayed in the text item.
 */
export function addTextItemToMenu(
    menu: HTMLElement, templateSelector: string, i18nText: I18nString): void {
  const labelOption = util.instantiateTemplate(templateSelector);
  dom.getFrom(labelOption, 'div', HTMLDivElement)
      .setAttribute('i18n-aria', i18nText);
  dom.getFrom(labelOption, 'span', HTMLSpanElement).textContent =
      loadTimeData.getI18nMessage(i18nText);
  menu.appendChild(labelOption);
}

/**
 * Clear all the items in the menu except the header.
 */
export function clearMenu(menu: ParentNode): void {
  const toBeRemoved = [];
  for (const child of menu.children) {
    if (child.classList.contains('menu-header')) {
      continue;
    }
    toBeRemoved.push(child);
  }
  for (const child of toBeRemoved) {
    menu.removeChild(child);
  }
}

/**
 * Gets the string with I18n from given photo resolution level.
 */
export function toPhotoResolutionOptionLabel(level: PhotoResolutionLevel):
    string {
  switch (level) {
    case PhotoResolutionLevel.FULL:
      return loadTimeData.getI18nMessage(I18nString.LABEL_FULL_RESOLUTION);
    case PhotoResolutionLevel.MEDIUM:
      return loadTimeData.getI18nMessage(I18nString.LABEL_MEDIUM_RESOLUTION);
    default:
      assertNotReached();
  }
}

/**
 * Gets the string with I18n from given aspect ratio set.
 */
export function toAspectRatioLabel(aspectRatioSet: AspectRatioSet): string {
  switch (aspectRatioSet) {
    case AspectRatioSet.RATIO_4_3:
      return '4:3';
    case AspectRatioSet.RATIO_16_9:
      return '16:9';
    case AspectRatioSet.RATIO_OTHER:
      return loadTimeData.getI18nMessage(I18nString.LABEL_OTHER_ASPECT_RATIO);
    case AspectRatioSet.RATIO_SQUARE:
      return loadTimeData.getI18nMessage(
          I18nString.LABEL_SWITCH_TAKE_SQUARE_PHOTO_BUTTON);
    default:
      assertNotReached();
  }
}

/**
 * Gets the aria string with I18n from given aspect ratio set.
 */
export function toAspectRatioAriaLabel(aspectRatioSet: AspectRatioSet): string {
  let ariaLabel;
  switch (aspectRatioSet) {
    case AspectRatioSet.RATIO_4_3:
      ariaLabel = I18nString.ARIA_ASPECT_RATIO_4_TO_3;
      break;
    case AspectRatioSet.RATIO_16_9:
      ariaLabel = I18nString.ARIA_ASPECT_RATIO_16_TO_9;
      break;
    case AspectRatioSet.RATIO_OTHER:
      ariaLabel = I18nString.LABEL_OTHER_ASPECT_RATIO;
      break;
    case AspectRatioSet.RATIO_SQUARE:
      ariaLabel = I18nString.LABEL_SWITCH_TAKE_SQUARE_PHOTO_BUTTON;
      break;
    default:
      assertNotReached();
  }
  return loadTimeData.getI18nMessage(ariaLabel);
}

/**
 * Gets the string with I18n from given video resolution level.
 */
export function toVideoResolutionOptionLabel(level: VideoResolutionLevel):
    string {
  let label;
  switch (level) {
    case VideoResolutionLevel.FOUR_K:
      label = I18nString.LABEL_VIDEO_RESOLUTION_4K;
      break;
    case VideoResolutionLevel.QUAD_HD:
      label = I18nString.LABEL_VIDEO_RESOLUTION_QUAD_HD;
      break;
    case VideoResolutionLevel.FULL_HD:
      label = I18nString.LABEL_VIDEO_RESOLUTION_FULL_HD;
      break;
    case VideoResolutionLevel.HD:
      label = I18nString.LABEL_VIDEO_RESOLUTION_HD;
      break;
    case VideoResolutionLevel.THREE_SIXTY_P:
      label = I18nString.LABEL_VIDEO_RESOLUTION_360P;
      break;
    case VideoResolutionLevel.FULL:
      label = I18nString.LABEL_FULL_RESOLUTION;
      break;
    case VideoResolutionLevel.MEDIUM:
      label = I18nString.LABEL_MEDIUM_RESOLUTION;
      break;
    default:
      assertNotReached();
  }
  return loadTimeData.getI18nMessage(label);
}
