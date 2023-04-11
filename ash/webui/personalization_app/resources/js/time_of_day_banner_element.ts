// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This component displays the Time of Day promo banner to the
 * user.
 */

import '../css/common.css.js';
import '../css/cros_button_style.css.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';

import {dismissTimeOfDayBanner} from './ambient/ambient_controller.js';
import {WithPersonalizationStore} from './personalization_store.js';
import {getTemplate} from './time_of_day_banner_element.html.js';

export class TimeOfDayBannerElement extends WithPersonalizationStore {
  static get is() {
    return 'time-of-day-banner';
  }

  static get template() {
    return getTemplate();
  }

  private onDismissClick_() {
    dismissTimeOfDayBanner(this.getStore());
  }
}

customElements.define(TimeOfDayBannerElement.is, TimeOfDayBannerElement);
