// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A polymer component that displays the SeaPen templates.
 */

import 'chrome://resources/ash/common/personalization/common.css.js';
import 'chrome://resources/ash/common/personalization/wallpaper_grid_item_element.js';
import 'chrome://resources/ash/common/personalization/wallpaper.css.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';

import {assert} from 'chrome://resources/js/assert.js';

import {getSeaPenTemplates, QUERY, SeaPenTemplate} from './constants.js';
import {logSeaPenTemplateSelect} from './sea_pen_metrics_logger.js';
import {SeaPenRouterElement} from './sea_pen_router_element.js';
import {WithSeaPenStore} from './sea_pen_store.js';
import {getTemplate} from './sea_pen_templates_element.html.js';
import {ChipToken, getDefaultOptions, getTemplateTokens, TemplateToken} from './sea_pen_utils.js';

export class SeaPenTemplatesElement extends WithSeaPenStore {
  static get is() {
    return 'sea-pen-templates';
  }
  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      seaPenTemplates_: {
        type: Array,
        value() {
          return getSeaPenTemplates();
        },
      },

      selected_: Object,

      hoveredTemplate_: Object,
    };
  }

  private seaPenTemplates_: SeaPenTemplate[];
  private selected_: SeaPenTemplate;
  private hoveredTemplate_: SeaPenTemplate|null;

  private getAriaIndex_(i: number): number {
    return i + 1;
  }

  private onTemplateSelected_(e: Event&{model: {template: SeaPenTemplate}}) {
    assert(e.model.template, 'no template selected');
    this.selected_ = e.model.template;
    const template = this.seaPenTemplates_.find(
        template => template.id === this.selected_.id);
    if (template) {
      // log metrics for the selected template.
      logSeaPenTemplateSelect(template.id);
      SeaPenRouterElement.instance().selectSeaPenTemplate(template.id);
    }
  }

  private onMouseOver_(e: Event&{model: {template: SeaPenTemplate}}) {
    this.hoveredTemplate_ = e.model.template;
  }

  private onMouseOut_() {
    this.hoveredTemplate_ = null;
  }

  private shouldShowTemplateTitle_(
      template: SeaPenTemplate|null, hoveredTemplate: SeaPenTemplate|null) {
    return template === hoveredTemplate && template?.id !== QUERY;
  }

  private getTemplateTokens_(template: SeaPenTemplate|null): TemplateToken[] {
    if (!template) {
      return [];
    }
    return getTemplateTokens(template, getDefaultOptions(template));
  }

  private isChip_(token: any): token is ChipToken {
    return typeof token?.translation === 'string';
  }
}
customElements.define(SeaPenTemplatesElement.is, SeaPenTemplatesElement);
