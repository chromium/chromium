// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let showDetails = false;

function $(id) {
  return document.body.querySelector(`#${id}`);
}

function updateDetails() {
  $('details').hidden = !showDetails;
}

function sendCommand(cmd) {
  if (window.supervisedUserErrorPageController) {
    switch (cmd) {
      case 'back':
        supervisedUserErrorPageController.goBack();
        break;
      case 'requestUrlAccessRemote':
        supervisedUserErrorPageController.requestUrlAccessRemote();
        break;
      case 'requestUrlAccessLocal':
        supervisedUserErrorPageController.requestUrlAccessLocal();
        break;
      case 'feedback':
        supervisedUserErrorPageController.feedback();
        break;
    }
    return;
  }
  window.domAutomationController.send(cmd);
}

function makeImageSet(url1x, url2x) {
  return '-webkit-image-set(url(' + url1x + ') 1x, url(' + url2x + ') 2x)';
}

function initialize() {
  const allowAccessRequests = loadTimeData.getBoolean('allowAccessRequests');
  const avatarURL1x = loadTimeData.getString('avatarURL1x');
  const avatarURL2x = loadTimeData.getString('avatarURL2x');
  const custodianName = loadTimeData.getString('custodianName');
  if (loadTimeData.getBoolean('isLocalWebApprovalsEnabled')) {
    console.error(
        'Local web approvals should not be enabled without web filter' +
        'interstitial refresh being enabled.');
    return;
  }

  if (custodianName && allowAccessRequests) {
    $('custodians-information').hidden = false;
    if (avatarURL1x) {
      $('custodian-avatar-img').style.content =
          makeImageSet(avatarURL1x, avatarURL2x);
    }
    $('custodian-name').textContent = custodianName;
    $('custodian-email').textContent = loadTimeData.getString('custodianEmail');
    const secondAvatarURL1x = loadTimeData.getString('secondAvatarURL1x');
    const secondAvatarURL2x = loadTimeData.getString('secondAvatarURL2x');
    const secondCustodianName = loadTimeData.getString('secondCustodianName');
    if (secondCustodianName) {
      $('second-custodian-information').hidden = false;
      $('second-custodian-avatar-img').hidden = false;
      if (secondAvatarURL1x) {
        $('second-custodian-avatar-img').style.content =
            makeImageSet(secondAvatarURL1x, secondAvatarURL2x);
      }
      $('second-custodian-name').textContent = secondCustodianName;
      $('second-custodian-email').textContent =
          loadTimeData.getString('secondCustodianEmail');
    }
  }

  const alreadyRequestedAccessRemote =
      loadTimeData.getBoolean('alreadySentRemoteRequest');
  if (alreadyRequestedAccessRemote) {
    const isMainFrame = loadTimeData.getBoolean('isMainFrame');
    requestCreated(true, isMainFrame);
    return;
  }

  if (allowAccessRequests) {
    $('remote-approvals-button').hidden = false;
    $('remote-approvals-button').onclick = function(event) {
      $('remote-approvals-button').disabled = true;
      sendCommand('requestUrlAccessRemote');
    };
  } else {
    $('remote-approvals-button').hidden = true;
  }

  if (loadTimeData.getBoolean('showFeedbackLink')) {
    $('show-details-link').hidden = false;
    $('show-details-link').onclick = function(event) {
      showDetails = true;
      $('show-details-link').hidden = true;
      $('hide-details-link').hidden = false;
      updateDetails();
    };
    $('hide-details-link').onclick = function(event) {
      showDetails = false;
      $('show-details-link').hidden = false;
      $('hide-details-link').hidden = true;
      updateDetails();
    };
    $('feedback-link').onclick = function(event) {
      sendCommand('feedback');
    };
  } else {
    $('feedback').hidden = true;
    $('details-button-container').hidden = true;
  }

  // Focus the top-level div for screen readers.
  $('frame-blocked').focus();
}

/**
 * Updates the interstitial to show that the request failed or was sent.
 * @param {boolean} isSuccessful Whether the request was successful or not.
 * @param {boolean} isMainFrame Whether the interstitial is being shown in main
 *     frame.
 */
function setRequestStatus(isSuccessful, isMainFrame) {
  console.log('setRequestStatus(' + isSuccessful + ')');
  requestCreated(isSuccessful, isMainFrame);
}

/**
 * Updates the interstitial to show that the request failed or was sent.
 * @param {boolean} isSuccessful Whether the request was successful or not.
 * @param {boolean} isMainFrame Whether the interstitial is being shown in main
 *     frame.
 */
function requestCreated(isSuccessful, isMainFrame) {
  $('block-page-header').hidden = true;
  $('block-page-message').hidden = true;
  $('hide-details-link').hidden = true;
  showDetails = false;
  updateDetails();
  if (isSuccessful) {
    $('request-failed-message').hidden = true;
    $('request-sent-message').hidden = false;
    $('remote-approvals-button').hidden = true;
    $('show-details-link').hidden = true;
    $('back-button').hidden = !isMainFrame;
    $('back-button').onclick = function(event) {
      sendCommand('back');
    };
    $('back-button').focus();
    $('error-page-illustration').hidden = true;
    $('waiting-for-approval-illustration').hidden = false;
  } else {
    $('request-failed-message').hidden = false;
    $('remote-approvals-button').disabled = false;
    $('show-details-link').hidden = false;
  }
}

document.addEventListener('DOMContentLoaded', initialize);
