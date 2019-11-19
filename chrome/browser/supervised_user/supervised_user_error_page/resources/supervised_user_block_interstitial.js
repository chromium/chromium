// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var mobileNav = false;

var showDetails = false;

/**
 * For small screen mobile the navigation buttons are moved
 * below the advanced text.
 */
function onResize() {
  var mediaQuery = '(min-width: 240px) and (max-width: 420px) and ' +
      '(max-height: 736px) and (min-height: 401px) and ' +
      '(orientation: portrait), (max-width: 736px) and ' +
      '(max-height: 420px) and (min-height: 240px) and ' +
      '(min-width: 421px) and (orientation: landscape)';

  // Check for change in nav status.
  if (mobileNav != window.matchMedia(mediaQuery).matches) {
    mobileNav = !mobileNav;
    updateDetails();
  }
}

function updateDetails() {
  $('information-container').hidden = mobileNav && showDetails;
  $('details').hidden = !showDetails;
}

function setupMobileNav() {
  window.addEventListener('resize', onResize);
  onResize();
}

document.addEventListener('DOMContentLoaded', setupMobileNav);

function sendCommand(cmd) {
  if (window.supervisedUserErrorPageController) {
    switch (cmd) {
      case 'back':
        supervisedUserErrorPageController.goBack();
        break;
      case 'request':
        supervisedUserErrorPageController.requestPermission();
        break;
      case 'feedback':
        supervisedUserErrorPageController.feedback();
        break;
    }
    return;
  }
  // TODO(bauerb): domAutomationController is not defined when this page is
  // shown in chrome://interstitials. Use a MessageHandler or something to
  // support interactions.
  window.domAutomationController.send(cmd);
}

function makeImageSet(url1x, url2x) {
  return '-webkit-image-set(url(' + url1x + ') 1x, url(' + url2x + ') 2x)';
}

function initialize() {
  var allowAccessRequests = loadTimeData.getBoolean('allowAccessRequests');
  if (allowAccessRequests) {
    $('request-access-button').onclick = function(event) {
      $('request-access-button').hidden = true;
      sendCommand('request');
    };
  } else {
    $('request-access-button').hidden = true;
  }
  var avatarURL1x = loadTimeData.getString('avatarURL1x');
  var avatarURL2x = loadTimeData.getString('avatarURL2x');
  var custodianName = loadTimeData.getString('custodianName');
  if (custodianName && allowAccessRequests) {
    $('custodians-information').hidden = false;
    if (avatarURL1x) {
      $('custodian-avatar-img').style.content =
          makeImageSet(avatarURL1x, avatarURL2x);
    }
    $('custodian-name').textContent = custodianName;
    $('custodian-email').textContent = loadTimeData.getString('custodianEmail');
    var secondAvatarURL1x = loadTimeData.getString('secondAvatarURL1x');
    var secondAvatarURL2x = loadTimeData.getString('secondAvatarURL2x');
    var secondCustodianName = loadTimeData.getString('secondCustodianName');
    if (secondCustodianName) {
      $('second-custodian-information').hidden = false;
      $('second-custodian-avatar-img').hidden = false;
      if (secondAvatarURL1x) {
        $('second-custodian-avatar-img').style.content =
            makeImageSet(secondAvatarURL1x, secondAvatarURL2x);
      }
      $('second-custodian-name').textContent = secondCustodianName;
      $('second-custodian-email').textContent = loadTimeData.getString(
          'secondCustodianEmail');
    }
  }
  $('back-button').onclick = function(event) {
    sendCommand('back');
  };
  if (loadTimeData.getBoolean('showFeedbackLink')) {
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
}

/**
 * Updates the interstitial to show that the request failed or was sent.
 * @param {boolean} isSuccessful Whether the request was successful or not.
 * @param {boolean} isMainFrame Whether the interstitial is being shown in main
 *     frame.
 */
function setRequestStatus(isSuccessful, isMainFrame) {
  console.log('setRequestStatus(' + isSuccessful +')');
  $('block-page-header').hidden = true;
  $('block-page-message').hidden = true;
  $('hide-details-link').hidden = true;
  showDetails = false;
  updateDetails();

  if (isSuccessful) {
    $('request-failed-message').hidden = true;
    $('request-sent-message').hidden = false;
    $('back-button').hidden = !isMainFrame;
    $('request-access-button').hidden = true;
    $('show-details-link').hidden = true;
  } else {
    $('request-failed-message').hidden = false;
    $('request-access-button').hidden = false;
    $('show-details-link').hidden = false;
  }
}

document.addEventListener('DOMContentLoaded', initialize);
