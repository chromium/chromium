// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let appWindow;
let appOrigin;

function generateContents(appIcon, appTitle, appPackageName) {
  const doc = document;
  const recommendAppsContainer = doc.getElementById('recommend-apps-container');
  const item = doc.createElement('div');
  item.classList.add('item');
  item.setAttribute('data-packagename', appPackageName);

  const chip = doc.createElement('div');
  chip.classList.add('chip');
  chip.tabIndex = 0;
  chip.addEventListener('mousedown', addRippleCircle_);
  chip.addEventListener('mouseup', toggleCheckStatus_);
  chip.addEventListener('animationend', removeRippleCircle_);

  // Add keyboard events
  let keyEventFired = false;
  chip.addEventListener('keydown', function(e) {
    if (!keyEventFired && isConfirmKey_(e))
      addRippleCircle_(e);
    keyEventFired = true;
  });
  chip.addEventListener('keyup', function(e) {
    if (isConfirmKey_(e)) {
      toggleCheckStatus_(e);
      keyEventFired = false;
    }
  });

  const chipContent = doc.createElement('div');
  chipContent.classList.add('chip-content-container');
  chipContent.tabIndex = -1;
  chip.appendChild(chipContent);

  item.appendChild(chip);

  const img = doc.createElement('img');
  img.classList.add('app-icon');
  img.setAttribute('src', decodeURIComponent(appIcon));

  const title = doc.createElement('span');
  title.classList.add('app-title');
  title.innerHTML = appTitle;

  chipContent.appendChild(img);
  chipContent.appendChild(title);

  const imagePicker = doc.createElement('div');
  imagePicker.classList.add('image-picker');
  imagePicker.addEventListener('click', toggleCheckStatus_);
  item.appendChild(imagePicker);

  recommendAppsContainer.appendChild(item);
}

/**
 * Add a layer on top of the chip to create the ripple effect.
 * @param {!Event} e
 * @private
 */
function addRippleCircle_(e) {
  const chip = e.currentTarget;
  const item = chip.parentNode;
  const offsetX = e.pageX - item.offsetLeft;
  const offsetY = e.pageY - item.offsetTop;
  chip.style.setProperty('--x', offsetX);
  chip.style.setProperty('--y', offsetY);
  const chipContent = chip.querySelector('.chip-content-container');
  chipContent.innerHTML += '<div class="ripple"></div>';
}

/**
 * After the animation ends, remove the ripple layer.
 * @param {!Event} e
 * @private
 */
function removeRippleCircle_(e) {
  const chip = e.currentTarget;
  const rippleLayers = chip.querySelectorAll('.ripple');
  for (const rippleLayer of rippleLayers) {
    if (rippleLayer.className === 'ripple') {
      rippleLayer.remove();
    }
  }
}

/**
 * Toggle the check status of an app. If an app is selected, add the "checked"
 * lass so that the checkmark is visible. Otherwise, remove the checked class.
 * @param {!Event} e
 * @private
 */
function toggleCheckStatus_(e) {
  const item = e.currentTarget.parentNode;
  item.classList.toggle('checked');

  sendNumberOfSelectedApps();
}

function getSelectedPackages() {
  const selectedPackages = [];
  const checkedItems = document.getElementsByClassName('checked');
  for (const checkedItem of checkedItems) {
    selectedPackages.push(checkedItem.dataset.packagename);
  }
  return selectedPackages;
}

function isConfirmKey_(e) {
  return e.keyCode === 13   // Enter
      || e.keyCode === 32;  // Space
}

/**
 * Send the number of selected apps back to the embedding page.
 */
function sendNumberOfSelectedApps() {
  if (appWindow && appOrigin) {
    const checkedItems = document.querySelectorAll('.checked');
    appWindow.postMessage(
        {type: 'NUM_OF_SELECTED_APPS', numOfSelected: checkedItems.length},
        appOrigin);
  }
}

function onMessage_(e) {
  appWindow = e.source;
  appOrigin = e.origin;
}

/**
 * Mark all recommended apps as checked.
 */
function selectAll() {
  const allItems = document.getElementsByClassName('item');
  for (const item of allItems) {
    item.classList.add('checked');
  }

  sendNumberOfSelectedApps();
}

/**
 * Calculate height of the recommend-apps-container.
 * @return {number}
 */
function getHeight() {
  return document.querySelector('#recommend-apps-container').clientHeight;
}

window.addEventListener('message', onMessage_);
