// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {ESimProfileProperties, ESimProfileRemote, EuiccRemote, ProfileState} from 'chrome://resources/mojo/chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom-webui.js';

import {getESimManagerRemote} from './mojo_interface_provider.js';

/**
 * Fetches the EUICC's eSIM profiles with status 'Pending'.
 * @param {!EuiccRemote} euicc
 * @return {!Promise<!Array<!ESimProfileRemote>>}
 */
export function getPendingESimProfiles(euicc) {
  return euicc.getProfileList().then(response => {
    return filterByProfileProperties_(response.profiles, properties => {
      return properties.state === ProfileState.kPending;
    });
  });
}

/**
 * Fetches the EUICC's eSIM profiles with status not 'Pending'.
 * @param {!EuiccRemote} euicc
 * @return {!Promise<!Array<!ESimProfileRemote>>}
 */
export function getNonPendingESimProfiles(euicc) {
  return euicc.getProfileList().then(response => {
    return filterByProfileProperties_(response.profiles, properties => {
      return properties.state !== ProfileState.kPending;
    });
  });
}

/**
 * Filters each profile in profiles by callback, which is given the profile's
 * properties as an argument and returns true or false. Does not guarantee
 * that profiles retains the same order.
 * @private
 * @param {!Array<!ESimProfileRemote>} profiles
 * @param {function(ESimProfileProperties)}
 *     callback
 * @return {!Promise<Array<!ESimProfileRemote>>}
 */
function filterByProfileProperties_(profiles, callback) {
  const profilePromises = profiles.map(profile => {
    return profile.getProperties().then(response => {
      if (!callback(response.properties)) {
        return null;
      }
      return profile;
    });
  });
  return Promise.all(profilePromises).then(profiles => {
    return profiles.filter(profile => {
      return profile !== null;
    });
  });
}

/**
 * @return {!Promise<number>}
 */
export function getNumESimProfiles() {
  return getEuicc()
      .then(euicc => {
        return euicc.getProfileList();
      })
      .then(response => {
        return response.profiles.length;
      });
}

/**
 * Returns the Euicc that should be used for eSim operations or null
 * if there is none available.
 * @return {!Promise<?EuiccRemote>}
 */
export async function getEuicc() {
  const eSimManagerRemote = getESimManagerRemote();
  const response = await eSimManagerRemote.getAvailableEuiccs();
  if (!response || !response.euiccs) {
    return null;
  }
  // Always use the first Euicc if Hermes only exposes one Euicc.
  // If useSecondEuicc flag is set and there are two Euicc available,
  // use the second available Euicc.
  if (response.euiccs.length === 0) {
    return null;
  }

  if (response.euiccs.length === 1) {
    return response.euiccs[0];
  }

  const euiccIndex = loadTimeData.getBoolean('useSecondEuicc') ? 1 : 0;
  return response.euiccs[euiccIndex];
}

/**
 * @param {string} iccid
 * @return {!Promise<?{
 *       profileRemote: ESimProfileRemote,
 *       profileProperties: ESimProfileProperties
 *     }>} Returns a eSIM profile remote and profile properties for given
 *         |iccid|.
 */
async function getESimProfileDetails(iccid) {
  if (!iccid) {
    return null;
  }
  const euicc = await getEuicc();

  if (!euicc) {
    console.error('No Euiccs found');
    return null;
  }
  const esimProfilesRemotes = await euicc.getProfileList();

  for (const profileRemote of esimProfilesRemotes.profiles) {
    const profilePropertiesResponse = await profileRemote.getProperties();
    if (!profilePropertiesResponse || !profilePropertiesResponse.properties) {
      return null;
    }

    const profileProperties = profilePropertiesResponse.properties;
    if (profileProperties.iccid === iccid) {
      return {profileRemote, profileProperties};
    }
  }
  return null;
}

/**
 * Returns the eSIM profile with iccid in the first EUICC or null if none
 * is found.
 * @param {string} iccid
 * @return {!Promise<?ESimProfileRemote>}
 */
export async function getESimProfile(iccid) {
  const details = await getESimProfileDetails(iccid);
  if (!details) {
    return null;
  }
  return details.profileRemote;
}

/**
 * Returns properties for eSIM profile with iccid in the first EUICC or null
 * if none is found.
 * @param {string} iccid
 * @return {!Promise<?ESimProfileProperties>}
 */
export async function getESimProfileProperties(iccid) {
  const details = await getESimProfileDetails(iccid);
  if (!details) {
    return null;
  }
  return details.profileProperties;
}
