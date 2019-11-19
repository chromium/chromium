// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('policy');

/**
 * @typedef {{
 *    [id: string]: {
 *      name: string,
 *      policyNames: !Array<string>,
 * }}
 */
policy.PolicyNamesResponse;

/**
 * @typedef {!Array<{
 *  name: string,
 *  id: ?String,
 *  policies: {[name: string]: policy.Policy}
 * }>}
 */
policy.PolicyValuesResponse;

/**
 * @typedef {{
 *    level: string,
 *    scope: string,
 *    source: string,
 *    value: any,
 * }}
 */
policy.Conflict;

/**
 * @typedef {{
 *    ignored?: boolean,
 *    name: string,
 *    level: string,
 *    link: ?string,
 *    scope: string,
 *    source: string,
 *    error: string,
 *    value: any,
 *    allSourcesMerged: ?boolean,
 *    conflicts: ?Array<!Conflict>,
 * }}
 */
policy.Policy;

/**
 * @typedef {{
 *     id: ?string,
 *     name: string,
 *     policies: !Array<!Policy>
 * }}
 */
policy.PolicyTableModel;

cr.define('policy', function() {
  /**
   * A box that shows the status of cloud policy for a device, machine or user.
   * @constructor
   * @extends {HTMLFieldSetElement}
   */
  const StatusBox = cr.ui.define(function() {
    const node = $('status-box-template').cloneNode(true);
    node.removeAttribute('id');
    return node;
  });

  StatusBox.prototype = {
    // Set up the prototype chain.
    __proto__: HTMLFieldSetElement.prototype,

    /**
     * Initialization function for the cr.ui framework.
     */
    decorate: function() {},

    /**
     * Sets the text of a particular named label element in the status box
     * and updates the visibility if needed.
     * @param {string} labelName The name of the label element that is being
     *     updated.
     * @param {string} labelValue The new text content for the label.
     * @param {boolean=} needsToBeShown True if we want to show the label
     *     False otherwise.
     */
    setLabelAndShow_: function(labelName, labelValue, needsToBeShown = true) {
      const labelElement = this.querySelector(labelName);
      labelElement.textContent = labelValue || '';
      if (needsToBeShown) {
        labelElement.parentElement.hidden = false;
      }
    },
    /**
     * Populate the box with the given cloud policy status.
     * @param {string} scope The policy scope, either "device", "machine", or
     *     "user".
     * @param {Object} status Dictionary with information about the status.
     */
    initialize: function(scope, status) {
      const notSpecifiedString = loadTimeData.getString('notSpecified');
      if (scope == 'device') {
        // For device policy, set the appropriate title and populate the topmost
        // status item with the domain the device is enrolled into.
        this.querySelector('.legend').textContent =
            loadTimeData.getString('statusDevice');
        this.setLabelAndShow_(
            '.enterprise-enrollment-domain', status.enterpriseEnrollmentDomain);
        this.setLabelAndShow_(
            '.enterprise-display-domain', status.enterpriseDisplayDomain);

        // Populate the device naming information.
        // Populate the asset identifier.
        this.setLabelAndShow_(
            '.asset-id', status.assetId || notSpecifiedString);

        // Populate the device location.
        this.setLabelAndShow_(
            '.location', status.location || notSpecifiedString);

        // Populate the directory API ID.
        this.setLabelAndShow_(
            '.directory-api-id', status.directoryApiId || notSpecifiedString);
        this.setLabelAndShow_('.client-id', status.clientId);
        //For off-hours policy, indicate if it's active or not.
        if (status.isOffHoursActive != null) {
          this.setLabelAndShow_(
              '.is-offhours-active',
              loadTimeData.getString(
                  status.isOffHoursActive ? 'offHoursActive' : 'offHoursNotActive'));
        }
      } else if (scope == 'machine') {
        // For machine policy, set the appropriate title and populate
        // machine enrollment status with the information that applies
        // to this machine.
        this.querySelector('.legend').textContent =
            loadTimeData.getString('statusMachine');
        this.setLabelAndShow_('.machine-enrollment-device-id', status.deviceId);
        this.setLabelAndShow_(
            '.machine-enrollment-token', status.enrollmentToken);
        this.setLabelAndShow_('.machine-enrollment-name', status.machine);
        this.setLabelAndShow_('.machine-enrollment-domain', status.domain);
      } else {
        // For user policy, set the appropriate title and populate the topmost
        // status item with the username that policies apply to.
        this.querySelector('.legend').textContent =
            loadTimeData.getString('statusUser');
        // Populate the topmost item with the username.
        this.setLabelAndShow_('.username', status.username);
        // Populate the user gaia id.
        this.setLabelAndShow_('.gaia-id', status.gaiaId || notSpecifiedString);
        this.setLabelAndShow_('.client-id', status.clientId);
        if (status.isAffiliated != null) {
          this.setLabelAndShow_(
              '.is-affiliated',
              loadTimeData.getString(
                  status.isAffiliated ? 'isAffiliatedYes' : 'isAffiliatedNo'));
        }
      }
      this.setLabelAndShow_(
          '.time-since-last-refresh', status.timeSinceLastRefresh, false);
      this.setLabelAndShow_('.refresh-interval', status.refreshInterval, false);
      this.setLabelAndShow_('.status', status.status, false);
      this.setLabelAndShow_(
          '.policy-push',
          loadTimeData.getString(
              status.policiesPushAvailable ? 'policiesPushOn' :
                                             'policiesPushOff'));
    },
  };

  /**
   * A single policy conflict's entry in the policy table.
   * @constructor
   * @extends {HTMLDivElement}
   */
  const PolicyConflict = cr.ui.define(function() {
    const node = $('policy-conflict-template').cloneNode(true);
    node.removeAttribute('id');
    return node;
  });

  PolicyConflict.prototype = {
    // Set up the prototype chain.
    __proto__: HTMLDivElement.prototype,

    decorate: function() {},

    /** @param {Conflict} conflict */
    initialize(conflict) {
      this.querySelector('.scope').textContent = loadTimeData.getString(
          conflict.scope == 'user' ? 'scopeUser' : 'scopeDevice');
      this.querySelector('.level').textContent = loadTimeData.getString(
          conflict.level == 'recommended' ? 'levelRecommended' :
                                            'levelMandatory');
      this.querySelector('.source').textContent =
          loadTimeData.getString(conflict.source);
      this.querySelector('.value.row .value').textContent = conflict.value;
    }
  };

  /**
   * A single policy's entry in the policy table.
   * @constructor
   * @extends {HTMLDivElement}
   */
  const Policy = cr.ui.define(function() {
    const node = $('policy-template').cloneNode(true);
    node.removeAttribute('id');
    return node;
  });

  Policy.prototype = {
    // Set up the prototype chain.
    __proto__: HTMLDivElement.prototype,

    /**
     * Initialization function for the cr.ui framework.
     */
    decorate: function() {
      const toggle = this.querySelector('.policy.row .toggle');
      toggle.addEventListener('click', this.toggleExpanded_.bind(this));
    },

    /** @param {Policy} policy */
    initialize(policy) {
      /** @type {Policy} */
      this.policy = policy;

      /** @private {boolean} */
      this.unset_ = policy.value === undefined;

      /** @private {boolean} */
      this.hasErrors_ = !!policy.error;

      /** @private {boolean} */
      this.hasWarnings_ = !!policy.warning;

      /** @private {boolean} */
      this.hasConflicts_ = !!policy.conflicts;

      /** @private {boolean} */
      this.isMergedValue_ = !!policy.allSourcesMerged;

      // Populate the name column.
      const nameDisplay = this.querySelector('.name .link span');
      nameDisplay.textContent = policy.name;
      if (policy.link) {
        const link = this.querySelector('.name .link');
        link.href = policy.link;
        link.title = loadTimeData.getStringF('policyLearnMore', policy.name);
      } else {
        this.classList.add('no-help-link');
      }

      // Populate the remaining columns with policy scope, level and value if a
      // value has been set. Otherwise, leave them blank.
      if (!this.unset_) {
        const scopeDisplay = this.querySelector('.scope');
        scopeDisplay.textContent = loadTimeData.getString(
            policy.scope == 'user' ? 'scopeUser' : 'scopeDevice');

        const levelDisplay = this.querySelector('.level');
        levelDisplay.textContent = loadTimeData.getString(
            policy.level == 'recommended' ? 'levelRecommended' :
                                            'levelMandatory');

        const sourceDisplay = this.querySelector('.source');
        sourceDisplay.textContent = loadTimeData.getString(policy.source);
        // Reduces load on the DOM for long values;
        const truncatedValue =
            (policy.value && policy.value.toString().length > 256) ?
            `${policy.value.toString().substr(0, 256)}\u2026` :
            policy.value;

        const valueDisplay = this.querySelector('.value');
        valueDisplay.textContent = truncatedValue;


        const valueRowContentDisplay = this.querySelector('.value.row .value');
        valueRowContentDisplay.textContent = policy.value;

        const errorRowContentDisplay = this.querySelector('.errors.row .value');
        errorRowContentDisplay.textContent = policy.error;
        const warningRowContentDisplay =
            this.querySelector('.warnings.row .value');
        warningRowContentDisplay.textContent = policy.warning;

        const messagesDisplay = this.querySelector('.messages');
        const errorsNotice =
            this.hasErrors_ ? loadTimeData.getString('error') : '';
        const warningsNotice =
            this.hasWarnings_ ? loadTimeData.getString('warning') : '';
        const conflictsNotice = this.hasConflicts_ && !this.isMergedValue_ ?
            loadTimeData.getString('conflict') :
            '';
        const ignoredNotice =
            this.policy.ignored ? loadTimeData.getString('ignored') : '';
        const notice =
            [errorsNotice, warningsNotice, ignoredNotice, conflictsNotice]
                .filter(x => !!x)
                .join(', ') ||
            loadTimeData.getString('ok');
        messagesDisplay.textContent = notice;


        if (policy.conflicts) {
          policy.conflicts.forEach(conflict => {
            const row = new PolicyConflict;
            row.initialize(conflict);
            this.appendChild(row);
          });
        }
      } else {
        const messagesDisplay = this.querySelector('.messages');
        messagesDisplay.textContent = loadTimeData.getString('unset');
      }
    },

    /**
     * Toggle the visibility of an additional row containing the complete text.
     * @private
     */
    toggleExpanded_: function() {
      const warningRowDisplay = this.querySelector('.warnings.row');
      const errorRowDisplay = this.querySelector('.errors.row');
      const valueRowDisplay = this.querySelector('.value.row');
      valueRowDisplay.hidden = !valueRowDisplay.hidden;
      if (valueRowDisplay.hidden) {
        this.classList.remove('expanded');
      } else {
        this.classList.add('expanded');
      }

      this.querySelector('.show-more').hidden = !valueRowDisplay.hidden;
      this.querySelector('.show-less').hidden = valueRowDisplay.hidden;
      if (this.hasWarnings_) {
        warningRowDisplay.hidden = !warningRowDisplay.hidden;
      }
      if (this.hasErrors_) {
        errorRowDisplay.hidden = !errorRowDisplay.hidden;
      }
      this.querySelectorAll('.policy-conflict-data')
          .forEach(row => row.hidden = !row.hidden);
    },
  };

  /**
   * A table of policies and their values.
   * @constructor
   * @extends {HTMLDivElement}
   */
  const PolicyTable = cr.ui.define(function() {
    const node = $('policy-table-template').cloneNode(true);
    node.removeAttribute('id');
    return node;
  });


  PolicyTable.prototype = {
    // Set up the prototype chain.
    __proto__: HTMLDivElement.prototype,

    /**
     * Initialization function for the cr.ui framework.
     */
    decorate: function() {
      this.policies_ = {};
      this.filterPattern_ = '';
    },

    /** @param {PolicyTableModel} dataModel */
    update(dataModel) {
      // Clear policies
      const mainContent = this.querySelector('.main');
      const policies = this.querySelectorAll('.policy-data');
      this.querySelector('.header').textContent = dataModel.name;
      this.querySelector('.id').textContent = dataModel.id;
      this.querySelector('.id').hidden = !dataModel.id;
      policies.forEach(row => mainContent.removeChild(row));

      dataModel.policies
          .sort((a, b) => {
            if ((a.value !== undefined && b.value !== undefined) ||
                a.value === b.value) {
              if (a.link !== undefined && b.link !== undefined) {
                // Sorting the policies in ascending alpha order.
                return a.name > b.name ? 1 : -1;
              }

              // Sorting so unknown policies are last.
              return a.link !== undefined ? -1 : 1;
            }

            // Sorting so unset values are last.
            return a.value !== undefined ? -1 : 1;
          })
          .forEach(policy => {
            const policyRow = new Policy;
            policyRow.initialize(policy);
            mainContent.appendChild(policyRow);
          });
      this.filter();
    },

    /**
     * Set the filter pattern. Only policies whose name contains |pattern| are
     * shown in the policy table. The filter is case insensitive. It can be
     * disabled by setting |pattern| to an empty string.
     * @param {string} pattern The filter pattern.
     */
    setFilterPattern: function(pattern) {
      this.filterPattern_ = pattern.toLowerCase();
      this.filter();
    },

    /**
     * Filter policies. Only policies whose name contains the filter pattern are
     * shown in the table. Furthermore, policies whose value is not currently
     * set are only shown if the corresponding checkbox is checked.
     */
    filter: function() {
      const showUnset = $('show-unset').checked;
      const policies = this.querySelectorAll('.policy-data');
      for (let i = 0; i < policies.length; i++) {
        const policyDisplay = policies[i];
        policyDisplay.hidden =
            policyDisplay.policy.value === undefined && !showUnset ||
            policyDisplay.policy.name.toLowerCase().indexOf(
                this.filterPattern_) === -1;
      }
      this.querySelector('.no-policy').hidden =
          !!this.querySelector('.policy-data:not([hidden])');
    },
  };

  /**
   * A singleton object that handles communication between browser and WebUI.
   * @constructor
   */
  function Page() {}

  // Make Page a singleton.
  cr.addSingletonGetter(Page);

  Page.prototype = {
    /**
     * Main initialization function. Called by the browser on page load.
     */
    initialize: function() {
      cr.ui.FocusOutlineManager.forDocument(document);

      this.mainSection = $('main-section');

      /** @type {{[id: string]: PolicyTable}} */
      this.policyTables = {};

      // Place the initial focus on the filter input field.
      $('filter').focus();

      const self = this;
      $('filter').onsearch = function(event) {
        for (policyTable in self.policyTables) {
          self.policyTables[policyTable].setFilterPattern(this.value);
        }
      };
      $('reload-policies').onclick = function(event) {
        this.disabled = true;
        chrome.send('reloadPolicies');
      };

      $('export-policies').onclick = function(event) {
        chrome.send('exportPoliciesJSON');
      };

      $('show-unset').onchange = function() {
        for (policyTable in self.policyTables) {
          self.policyTables[policyTable].filter();
        }
      };

      chrome.send('listenPoliciesUpdates');
      cr.addWebUIListener('status-updated', status => this.setStatus(status));
      cr.addWebUIListener(
          'policies-updated',
          (names, values) => this.onPoliciesReceived_(names, values));
    },

    /**
     * @param {PolicyNamesResponse} policyNames
     * @param {PolicyValuesResponse} policyValues
     * @private
     */
    onPoliciesReceived_(policyNames, policyValues) {
      /** @type {Array<!PolicyTableModel>} */ const policyGroups =
          policyValues.map(value => {
            const knownPolicyNames =
                (policyNames[value.id] || policyNames.chrome).policyNames;
            const knownPolicyNamesSet = new Set(knownPolicyNames);
            const receivedPolicyNames = Object.keys(value.policies);
            const allPolicyNames = Array.from(
                new Set([...knownPolicyNames, ...receivedPolicyNames]));
            const policies = allPolicyNames.map(
                name => Object.assign(
                    {
                      name,
                      link:
                          knownPolicyNames === policyNames.chrome.policyNames &&
                              knownPolicyNamesSet.has(name) ?
                          `https://cloud.google.com/docs/chrome-enterprise/policies/?policy=${
                              name}` :
                          undefined,
                    },
                    value.policies[name]));

            return {name: value.name, id: value.id, policies};
          });

      policyGroups.forEach(group => this.createOrUpdatePolicyTable(group));

      this.reloadPoliciesDone();
    },

    /** @param {PolicyTableModel} dataModel */
    createOrUpdatePolicyTable(dataModel) {
      const id = `${dataModel.name}-${dataModel.id}`;
      if (!this.policyTables[id]) {
        this.policyTables[id] = new PolicyTable;
        this.mainSection.appendChild(this.policyTables[id]);
      }
      this.policyTables[id].update(dataModel);
    },

    /**
     * Update the status section of the page to show the current cloud policy
     * status.
     * @param {Object} status Dictionary containing the current policy status.
     */
    setStatus: function(status) {
      // Remove any existing status boxes.
      const container = $('status-box-container');
      while (container.firstChild) {
        container.removeChild(container.firstChild);
      }
      // Hide the status section.
      const section = $('status-section');
      section.hidden = true;

      // Add a status box for each scope that has a cloud policy status.
      for (const scope in status) {
        const box = new StatusBox;
        box.initialize(scope, status[scope]);
        container.appendChild(box);
        // Show the status section.
        section.hidden = false;
      }
    },

    /**
     * Re-enable the reload policies button when the previous request to reload
     * policies values has completed.
     */
    reloadPoliciesDone: function() {
      $('reload-policies').disabled = false;
    },
  };

  return {Page: Page, PolicyTable: PolicyTable, Policy: Policy};
});
