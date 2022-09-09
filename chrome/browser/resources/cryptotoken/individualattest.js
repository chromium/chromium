// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides an interface to determine whether to request the
 * individual attestation certificate during enrollment.
 */
'use strict';

/**
 * Interface to determine whether to request the individual attestation
 * certificate during enrollment.
 * @interface
 */
function IndividualAttestation() {}

/**
 * @param {string} appIdHash The app id hash.
 * @return {boolean} Whether to request the individual attestation certificate
 *     for this app id.
 */
IndividualAttestation.prototype.requestIndividualAttestation = function(
    appIdHash) {};
