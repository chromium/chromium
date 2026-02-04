// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './enterprise_policy_value.css.js';
import {getHtml} from './enterprise_policy_value.html.js';

/**
 * An error indicating that a policy value could not be formatted as expected.
 */
class FormatError extends Error {}

export class EnterprisePolicyValueElement extends CrLitElement {
  static get is() {
    return 'enterprise-policy-value';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      policyName: {type: String},
      value: {type: Object},
    };
  }

  accessor policyName: string = '';
  accessor value: string|number|Record<string, unknown> = '';

  protected formattedValue: string = '';

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    if (changedProperties.has('policyName') || changedProperties.has('value')) {
      this.formattedValue = this.computeFormattedValue();
    }
  }

  private computeFormattedValue(): string {
    try {
      switch (this.policyName) {
        case 'LastCheckPeriod':
          return this.computeFormattedLastCheckPeriod();
        case 'UpdatesSuppressed':
          return this.computeFormattedUpdatesSuppressed();
        case 'PackageCacheSizeLimit':
          return this.computeFormattedPackageCacheSizeLimit();
        case 'PackageCacheExpires':
          return this.computeFormattedPackageCacheExpires();
        default:
          break;
      }
    } catch (e) {
      if (e instanceof FormatError) {
        console.warn(`Failed to format ${this.policyName}: ${e}`);
      } else {
        throw e;
      }
    }

    switch (typeof this.value) {
      case 'string':
        return this.value;
      case 'number':
        return String(this.value);
      default:
        return JSON.stringify(this.value);
    }
  }

  private computeFormattedLastCheckPeriod(): string {
    const microseconds = Number(this.value);
    if (Number.isNaN(microseconds)) {
      throw new FormatError(`Expected ${this.value} to be a Number`);
    }
    let remaining = Math.floor(microseconds / 1000000);
    const seconds = remaining % 60;
    remaining = Math.floor(remaining / 60);
    const minutes = remaining % 60;
    remaining = Math.floor(remaining / 60);
    const hours = remaining % 24;
    const days = Math.floor(remaining / 24);
    return new Intl
        .DurationFormat(undefined, {
          style: 'long',
        })
        .format({days, hours, minutes, seconds});
  }

  private computeFormattedUpdatesSuppressed(): string {
    if (typeof this.value !== 'object') {
      throw new FormatError(`Expected ${this.value} to be an Object`);
    }
    const startHour = Number(this.value['StartHour']);
    if (Number.isNaN(startHour)) {
      throw new FormatError(
          `Expected StartHour to be a Number, got ${this.value['StartHour']}`);
    }
    const startMinute = Number(this.value['StartMinute']);
    if (Number.isNaN(startMinute)) {
      throw new FormatError(`Expected StartMinute to be a Number, got ${
          this.value['StartMinute']}`);
    }
    const durationMinutes = Number(this.value['Duration']);
    if (Number.isNaN(durationMinutes)) {
      throw new FormatError(
          `Expected Duration to be a Number, got ${this.value['Duration']}`);
    }

    // Use Intl.DateTimeFormat to present the time range in a localized format.
    // Omitting formatting options for the day, month, and year will allow them
    // to be hidden by default. However, if the range spans multiple days (e.g.
    // 10:00 PM - 3:00 AM), formatRange will present date pieces in an attempt
    // to disambiguate. To avoid this, the end date is normalized to be on the
    // same day as the start date.
    const startDate = new Date();
    startDate.setHours(startHour, startMinute);
    const endDate = new Date();
    endDate.setHours(startHour, startMinute + durationMinutes);
    const normalizedEndDate = new Date(startDate);
    normalizedEndDate.setHours(endDate.getHours(), endDate.getMinutes());
    return new Intl
        .DateTimeFormat(undefined, {
          hour: 'numeric',
          minute: 'numeric',
        })
        .formatRange(startDate, normalizedEndDate);
  }

  private computeFormattedPackageCacheSizeLimit(): string {
    const megabytes = Number(this.value);
    if (Number.isNaN(megabytes)) {
      throw new FormatError(`Expected ${this.value} to be a Number`);
    }
    return new Intl
        .NumberFormat(undefined, {
          style: 'unit',
          unit: 'megabyte',
          unitDisplay: 'short',
        })
        .format(megabytes);
  }

  private computeFormattedPackageCacheExpires(): string {
    const days = Number(this.value);
    if (Number.isNaN(days)) {
      throw new FormatError(`Expected ${this.value} to be a Number`);
    }
    return new Intl
        .DurationFormat(undefined, {
          style: 'long',
        })
        .format({days});
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'enterprise-policy-value': EnterprisePolicyValueElement;
  }
}

customElements.define(
    EnterprisePolicyValueElement.is, EnterprisePolicyValueElement);
