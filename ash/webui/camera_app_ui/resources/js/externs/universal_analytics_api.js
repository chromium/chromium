/*
 * Copyright 2019 The Closure Compiler Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* eslint-disable */
/**
 * @fileoverview Externs for the Universal Analytics API (analytics.js).
 *
 * @externs
 */


/**
 * https://developers.google.com/analytics/devguides/collection/analyticsjs/command-queue-reference
 * @param {string|function(!ga.Tracker)} commandOrReadyCallback
 * @param {string|!ga.Fields=} field1
 * @param {string|?ga.Fields|function(!ga.Model)=} field2
 * @param {string|!ga.Fields=} field3
 * @param {number|string|!ga.Fields=} field4
 * @param {number|string|!ga.Fields=} field5
 * @param {!ga.Fields=} field6
 * @suppress {duplicate} Enables defining a stub for ga() until analytics.js is
 *     loaded. See
 *     https://developers.google.com/analytics/devguides/collection/analyticsjs/#alternative_async_tracking_snippet
 * @const
 */
var ga = function(
    commandOrReadyCallback, field1, field2, field3, field4, field5, field6) {};

/**
 * https://developers.google.com/analytics/devguides/collection/analyticsjs/ga-object-methods-reference#getAll
 * @return {!Array<!ga.Tracker>}
 */
ga.getAll = function() {};

/**
 * https://developers.google.com/analytics/devguides/collection/analyticsjs/ga-object-methods-reference#getByName
 * @param {string} name
 * @return {!ga.Tracker|undefined}
 */
ga.getByName = function(name) {};

/**
 * https://developers.google.com/analytics/devguides/collection/analyticsjs/ga-object-methods-reference#create
 * @param {string} trackingId
 * @param {string|!ga.Fields=} cookieDomainOrFields
 * @param {string|!ga.Fields=} nameOrFields
 * @param {!ga.Fields=} fields
 * @return {!ga.Tracker}
 */
ga.create = function(trackingId, cookieDomainOrFields, nameOrFields, fields) {};

/**
 * https://developers.google.com/analytics/devguides/collection/analyticsjs/ga-object-methods-reference#remove
 * @param {string} name
 */
ga.remove = function(name) {};

/**
 * https://developers.google.com/analytics/devguides/collection/analyticsjs/model-object-reference
 * @interface
 */
ga.Model = class {
  /**
   * https://developers.google.com/analytics/devguides/collection/analyticsjs/model-object-reference#get
   * @param {string} fieldName
   * @return {?}
   */
  get(fieldName) {}

  /**
   * https://developers.google.com/analytics/devguides/collection/analyticsjs/model-object-reference#set
   * @param {string|!ga.Fields} fieldNameOrObject
   * @param {?boolean|number|string|function()} fieldValue
   * @param {boolean=} temporary
   */
  set(fieldNameOrObject, fieldValue, temporary) {}
};

/**
 * https://developers.google.com/analytics/devguides/collection/analyticsjs/tracker-object-reference
 * @interface
 */
ga.Tracker = class {
  /**
   * https://developers.google.com/analytics/devguides/collection/analyticsjs/tracker-object-reference#get
   * @param {string} fieldName
   * @return {?}
   */
  get(fieldName) {}

  /**
   * https://developers.google.com/analytics/devguides/collection/analyticsjs/tracker-object-reference#set
   * @param {string|!ga.Fields} fieldNameOrObject
   * @param {?boolean|number|string|function()|function(!ga.Model)} fieldValue
   */
  set(fieldNameOrObject, fieldValue) {}

  /**
   * https://developers.google.com/analytics/devguides/collection/analyticsjs/tracker-object-reference#send
   * @param {string} hitType
   * @param {string|!ga.Fields=} field1
   * @param {string|!ga.Fields=} field2
   * @param {number|string|!ga.Fields=} field3
   * @param {number|string|!ga.Fields=} field4
   * @param {!ga.Fields=} field5
   */
  send(hitType, field1, field2, field3, field4, field5) {}
};

/**
 * https://developers.google.com/analytics/devguides/collection/analyticsjs/field-reference
 * See cl/249045133's description for explanation how this class was generated.
 * @record
 */
ga.Fields = class {
  constructor() {
    /** @type {?string|undefined} */
    this.action;

    /** @type {?string|undefined} */
    this.affiliation;

    /** @type {?boolean|undefined} */
    this.allowAdFeatures;

    /** @type {?boolean|undefined} */
    this.allowAnchor;

    /** @type {?boolean|undefined} */
    this.allowLinker;

    /** @type {?boolean|undefined} */
    this.alwaysSendReferrer;

    /** @type {?boolean|undefined} */
    this.anonymizeIp;

    /** @type {?string|undefined} */
    this.appId;

    /** @type {?string|undefined} */
    this.appInstallerId;

    /** @type {?string|undefined} */
    this.appName;

    /** @type {?string|undefined} */
    this.appVersion;

    /** @type {?string|undefined} */
    this.brand;

    /** @type {?string|undefined} */
    this.campaignContent;

    /** @type {?string|undefined} */
    this.campaignId;

    /** @type {?string|undefined} */
    this.campaignKeyword;

    /** @type {?string|undefined} */
    this.campaignMedium;

    /** @type {?string|undefined} */
    this.campaignName;

    /** @type {?string|undefined} */
    this.campaignSource;

    /** @type {?string|undefined} */
    this.category;

    /** @type {?string|undefined} */
    this.clientId;

    /** @type {?string|undefined} */
    this.contentGroup1;

    /** @type {?string|undefined} */
    this.contentGroup2;

    /** @type {?string|undefined} */
    this.contentGroup3;

    /** @type {?string|undefined} */
    this.contentGroup4;

    /** @type {?string|undefined} */
    this.contentGroup5;

    /** @type {?string|undefined} */
    this.cookieDomain;

    /** @type {?number|undefined} */
    this.cookieExpires;

    /** @type {?string|undefined} */
    this.cookieName;

    /** @type {?string|undefined} */
    this.coupon;

    /** @type {?string|undefined} */
    this.creative;

    /** @type {?string|undefined} */
    this.currencyCode;

    /** @type {?string|undefined} */
    this.dataSource;

    /** @type {?string|undefined} */
    this.dimension1;

    /** @type {?string|undefined} */
    this.dimension2;

    /** @type {?string|undefined} */
    this.dimension3;

    /** @type {?string|undefined} */
    this.dimension4;

    /** @type {?string|undefined} */
    this.dimension5;

    /** @type {?string|undefined} */
    this.dimension6;

    /** @type {?string|undefined} */
    this.dimension7;

    /** @type {?string|undefined} */
    this.dimension8;

    /** @type {?string|undefined} */
    this.dimension9;

    /** @type {?string|undefined} */
    this.dimension10;

    /** @type {?string|undefined} */
    this.dimension11;

    /** @type {?string|undefined} */
    this.dimension12;

    /** @type {?string|undefined} */
    this.dimension13;

    /** @type {?string|undefined} */
    this.dimension14;

    /** @type {?string|undefined} */
    this.dimension15;

    /** @type {?string|undefined} */
    this.dimension16;

    /** @type {?string|undefined} */
    this.dimension17;

    /** @type {?string|undefined} */
    this.dimension18;

    /** @type {?string|undefined} */
    this.dimension19;

    /** @type {?string|undefined} */
    this.dimension20;

    /** @type {?string|undefined} */
    this.encoding;

    /** @type {?string|undefined} */
    this.eventAction;

    /** @type {?string|undefined} */
    this.eventCategory;

    /** @type {?string|undefined} */
    this.eventLabel;

    /** @type {?number|undefined} */
    this.eventValue;

    /** @type {?string|undefined} */
    this.exDescription;

    /** @type {?boolean|undefined} */
    this.exFatal;

    /** @type {?string|undefined} */
    this.expId;

    /** @type {?string|undefined} */
    this.expVar;

    /** @type {?string|undefined} */
    this.flashVersion;

    /** @type {?boolean|undefined} */
    this.forceSSL;

    /** @type {?function()|undefined} */
    this.hitCallback;

    /** @type {?string|undefined} */
    this.hitType;

    /** @type {?string|undefined} */
    this.hostname;

    /** @type {?string|undefined} */
    this.id;

    /** @type {?boolean|undefined} */
    this.javaEnabled;

    /** @type {?string|undefined} */
    this.language;

    /** @type {?string|undefined} */
    this.legacyCookieDomain;

    /** @type {?boolean|undefined} */
    this.legacyHistoryImport;

    /** @type {?string|undefined} */
    this.linkerParam;

    /** @type {?string|undefined} */
    this.linkid;

    /** @type {?string|undefined} */
    this.list;

    /** @type {?string|undefined} */
    this.location;

    /** @type {?number|undefined} */
    this.metric1;

    /** @type {?number|undefined} */
    this.metric2;

    /** @type {?number|undefined} */
    this.metric3;

    /** @type {?number|undefined} */
    this.metric4;

    /** @type {?number|undefined} */
    this.metric5;

    /** @type {?number|undefined} */
    this.metric6;

    /** @type {?number|undefined} */
    this.metric7;

    /** @type {?number|undefined} */
    this.metric8;

    /** @type {?number|undefined} */
    this.metric9;

    /** @type {?number|undefined} */
    this.metric10;

    /** @type {?number|undefined} */
    this.metric11;

    /** @type {?number|undefined} */
    this.metric12;

    /** @type {?number|undefined} */
    this.metric13;

    /** @type {?number|undefined} */
    this.metric14;

    /** @type {?number|undefined} */
    this.metric15;

    /** @type {?number|undefined} */
    this.metric16;

    /** @type {?number|undefined} */
    this.metric17;

    /** @type {?number|undefined} */
    this.metric18;

    /** @type {?number|undefined} */
    this.metric19;

    /** @type {?number|undefined} */
    this.metric20;

    /** @type {?string|undefined} */
    this.name;

    /** @type {?boolean|undefined} */
    this.nonInteraction;

    /** @type {?string|undefined} */
    this.option;

    /** @type {?string|undefined} */
    this.page;

    /** @type {?number|string|undefined} */
    this.position;

    /** @type {?string|undefined} */
    this.price;

    /** @type {?number|undefined} */
    this.quantity;

    /** @type {?number|undefined} */
    this.queueTime;

    /** @type {?string|undefined} */
    this.referrer;

    /** @type {?string|undefined} */
    this.revenue;

    /** @type {?number|undefined} */
    this.sampleRate;

    /** @type {?string|undefined} */
    this.screenColors;

    /** @type {?string|undefined} */
    this.screenName;

    /** @type {?string|undefined} */
    this.screenResolution;

    /** @type {?string|undefined} */
    this.sessionControl;

    /** @type {?string|undefined} */
    this.shipping;

    /** @type {?number|undefined} */
    this.siteSpeedSampleRate;

    /** @type {?string|undefined} */
    this.socialAction;

    /** @type {?string|undefined} */
    this.socialNetwork;

    /** @type {?string|undefined} */
    this.socialTarget;

    /** @type {?number|undefined} */
    this.step;

    /** @type {?boolean|undefined} */
    this.storeGac;

    /** @type {?string|undefined} */
    this.tax;

    /** @type {?string|undefined} */
    this.timingCategory;

    /** @type {?string|undefined} */
    this.timingLabel;

    /** @type {?number|undefined} */
    this.timingValue;

    /** @type {?string|undefined} */
    this.timingVar;

    /** @type {?string|undefined} */
    this.title;

    /** @type {?string|undefined} */
    this.trackingId;

    /** @type {?string|undefined} */
    this.transport;

    /** @type {?boolean|undefined} */
    this.useBeacon;

    /** @type {?string|undefined} */
    this.userId;

    /** @type {?string|undefined} */
    this.variant;

    /** @type {?string|undefined} */
    this.viewportSize;
  }
};