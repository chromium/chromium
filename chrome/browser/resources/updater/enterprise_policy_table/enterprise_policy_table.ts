// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './enterprise_policy_table_section.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import type {PolicyData, PolicySet} from '../event_history.js';
import {loadTimeData} from '../i18n_setup.js';
import {getKnownAppNamesById} from '../known_apps.js';
import {deepEqual} from '../tools.js';

import {getCss} from './enterprise_policy_table.css.js';
import {getHtml} from './enterprise_policy_table.html.js';
import type {RowData} from './enterprise_policy_table_section.js';

export class EnterprisePolicyTableElement extends CrLitElement {
  static get is() {
    return 'enterprise-policy-table';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      policies: {type: Object},
      appId: {type: String},
    };
  }

  accessor policies: PolicySet|undefined = undefined;
  accessor appId: string|undefined = undefined;

  protected hasOnlyDefaultValues = true;
  protected updaterPolicies: RowData[] = [];
  protected appPolicies: {[label: string]: RowData[]} = {};

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    if (changedProperties.has('policies') || changedProperties.has('appId')) {
      this.hasOnlyDefaultValues = this.computeHasOnlyDefaultValues();
      this.updaterPolicies = this.computeUpdaterPolicies();
      this.appPolicies = this.computeAppPolicies();
    }
  }

  protected computeHasOnlyDefaultValues(): boolean {
    if (!this.policies) {
      return true;
    }

    const hasNonDefault = (policies: {[key: string]: PolicyData}) =>
        Object.values(policies).some(policy => {
          const sources = Object.keys(policy.valuesBySource);
          return sources.length !== 1 ||
              sources[0]!.toLowerCase() !== 'default';
        });

    if (hasNonDefault(this.policies.policiesByName)) {
      return false;
    }

    for (const [appId, appPolicies] of Object.entries(
             this.policies.policiesByAppId)) {
      if (this.appId !== undefined &&
          appId.toLowerCase() !== this.appId.toLowerCase()) {
        continue;
      }
      if (hasNonDefault(appPolicies)) {
        return false;
      }
    }

    return true;
  }

  protected computeUpdaterPolicies(): RowData[] {
    if (this.policies === undefined) {
      return [];
    }
    return this.makeRowData(this.policies.policiesByName);
  }

  protected computeAppPolicies(): {[key: string]: RowData[]} {
    if (this.policies === undefined) {
      return {};
    }

    let entries = Object.entries(this.policies.policiesByAppId);
    if (this.appId !== undefined) {
      entries = entries.filter(
          ([appId, _]) => appId.toLowerCase() === this.appId!.toLowerCase());
    }

    return Object.fromEntries(entries.map(
        ([
          appId,
          policiesByName,
        ]) => [this.getAppLabel(appId), this.makeRowData(policiesByName)]));
  }

  private makeRowData(policiesByName: {[key: string]: PolicyData}): RowData[] {
    const hasConflict = (policy: PolicyData): boolean => {
      const nonDefaultValues =
          Object.entries(policy.valuesBySource)
              .filter(([source, _]) => source.toLowerCase() !== 'default')
              .map(([_, value]) => value);
      return nonDefaultValues.some(v => !deepEqual(v, nonDefaultValues[0]));
    };
    return Object.entries(policiesByName).map(([name, policy]) => ({
                                                name,
                                                policy,
                                                isExpanded: false,
                                                hasConflict:
                                                    hasConflict(policy),
                                              }));
  }

  private getAppLabel(appId: string): string {
    return getKnownAppNamesById().get(appId.toLowerCase()) ?? appId;
  }

  protected getAppPoliciesLabel(appLabel: string): string {
    return loadTimeData.getStringF('appPolicies', appLabel);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'enterprise-policy-table': EnterprisePolicyTableElement;
  }
}

customElements.define(
    EnterprisePolicyTableElement.is, EnterprisePolicyTableElement);
