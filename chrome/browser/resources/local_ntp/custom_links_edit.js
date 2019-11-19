/* Copyright 2018 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

/**
 * Enum for ids.
 * @enum {string}
 * @const
 */
const IDS = {
  CANCEL: 'cancel',                      // Cancel button.
  DELETE: 'delete',                      // Delete button.
  DIALOG_TITLE: 'dialog-title',          // Dialog title.
  DONE: 'done',                          // Done button.
  EDIT_DIALOG: 'edit-link-dialog',       // Dialog element.
  FORM: 'edit-form',                     // The edit link form.
  INVALID_URL: 'invalid-url',            // Invalid URL error message.
  TITLE_FIELD: 'title-field',            // Title input field.
  TITLE_FIELD_NAME: 'title-field-name',  // Title input field name.
  URL_FIELD: 'url-field',                // URL input field.
  URL_FIELD_CONTAINER: 'url',            // URL input field container.
  URL_FIELD_NAME: 'url-field-name',      // URL input field name.
};

/**
 * Enum for key codes.
 * @enum {number}
 * @const
 */
const KEYCODES = {
  ENTER: 13,
  ESC: 27,
  SPACE: 32,
  TAB: 9,
};

/**
 * The origin of this request, i.e. 'https://www.google.TLD' for the remote NTP,
 * or 'chrome-search://local-ntp' for the local NTP.
 * @const {string}
 */
const DOMAIN_ORIGIN = '{{ORIGIN}}';

/**
 * List of parameters passed by query args.
 * @type {Object}
 */
let queryArgs = {};

/**
 * The prepopulated data for the form. Includes title, url, and rid.
 * @type {Object}
 */
const prepopulatedLink = {
  rid: -1,
  title: '',
  url: '',
};

/**
 * The title of the dialog when adding a link.
 * @type {string}
 */
let addLinkTitle = '';

/**
 * The title of the dialog when editing a link.
 * @type {string}
 */
let editLinkTitle = '';

/**
 * The accessibility title of remove link button.
 * @type {string}
 */
let deleteLinkTitle = '';

/**
 * Handler for the 'linkData' message from the host page. Pre-populates the url
 * and title fields with link's data obtained using the rid. Called if we are
 * editing an existing link.
 * @param {number} rid Restricted id of the link to be edited.
 */
function prepopulateFields(rid) {
  if (!isFinite(rid)) {
    return;
  }

  // Grab the link data from the embeddedSearch API.
  const data = chrome.embeddedSearch.newTabPage.getMostVisitedItemData(rid);
  if (!data) {
    return;
  }
  prepopulatedLink.rid = rid;
  $(IDS.TITLE_FIELD).value = prepopulatedLink.title = data.title;
  $(IDS.TITLE_FIELD).dir = data.direction || 'ltr';
  $(IDS.URL_FIELD).value = prepopulatedLink.url = data.url;

  // Set accessibility names.
  $(IDS.DELETE).setAttribute('aria-label', deleteLinkTitle + ' ' + data.title);
  $(IDS.DONE).setAttribute('aria-label', editLinkTitle + ' ' + data.title);
  $(IDS.DONE).title = editLinkTitle;
}

/**
 * Shows the invalid URL error message until the URL field is modified.
 */
function showInvalidUrlUntilTextInput() {
  $(IDS.URL_FIELD_CONTAINER).classList.add('invalid');
  const reenable = (event) => {
    $(IDS.URL_FIELD_CONTAINER).classList.remove('invalid');
    $(IDS.URL_FIELD).removeEventListener('input', reenable);
  };
  $(IDS.URL_FIELD).addEventListener('input', reenable);
}

/**
 * Send a message to close the edit dialog. Called when the edit flow has been
 * completed. If the fields were unchanged, does not update the link data.
 */
function finishEditLink() {
  let newUrl = '';
  let newTitle = '';

  const urlValue = $(IDS.URL_FIELD).value;
  if (urlValue != prepopulatedLink.url) {
    newUrl = chrome.embeddedSearch.newTabPage.fixupAndValidateUrl(urlValue);
    // Show error message for invalid urls.
    if (!newUrl || (newUrl && !utils.isSchemeAllowed(newUrl))) {
      showInvalidUrlUntilTextInput();
      $(IDS.DONE).disabled = true;  // Disable submit until text input.
      return;
    }
  }

  const titleValue = $(IDS.TITLE_FIELD).value;
  if (!titleValue) {  // Set the URL input as the title if no title is provided.
    newTitle = urlValue;
  } else if (titleValue != prepopulatedLink.title) {
    newTitle = titleValue;
  }

  // Update the link only if a field was changed.
  if (newUrl || newTitle) {
    chrome.embeddedSearch.newTabPage.updateCustomLink(
        prepopulatedLink.rid, newUrl, newTitle);
  }
  closeDialog();
}

/**
 * Call the EmbeddedSearchAPI to delete the link. Closes the dialog.
 * @param {!Event} event The click event.
 */
function deleteLink(event) {
  chrome.embeddedSearch.newTabPage.deleteMostVisitedItem(prepopulatedLink.rid);
  closeDialog();
}

/**
 * Send a message to close the edit dialog, clears the url and title fields, and
 * resets the button statuses. Called when the edit flow has been completed.
 */
function closeDialog() {
  window.parent.postMessage({cmd: 'closeDialog'}, DOMAIN_ORIGIN);
  // Small delay to allow the dialog to close before cleaning up.
  window.setTimeout(() => {
    $(IDS.FORM).reset();
    $(IDS.TITLE_FIELD).dir = '';
    $(IDS.URL_FIELD_CONTAINER).classList.remove('invalid');
    $(IDS.DELETE).disabled = false;
    $(IDS.DONE).disabled = false;
    prepopulatedLink.rid = -1;
    prepopulatedLink.title = '';
    prepopulatedLink.url = '';
  }, 10);
}

/**
 * Send a message to refocus the edited tile's three dot menu or the add
 * shortcut tile after the cancel button is clicked.
 * @param {Event} event The keydown event
 */
function focusBackOnCancel(event) {
  if (event.keyCode === KEYCODES.ENTER || event.keyCode === KEYCODES.SPACE) {
    const message = {cmd: 'focusMenu', rid: prepopulatedLink.rid};
    window.parent.postMessage(message, DOMAIN_ORIGIN);
    event.preventDefault();
    closeDialog();
  }
}

/**
 * Event handler for messages from the host page.
 * @param {Event} event Event received.
 */
function handlePostMessage(event) {
  const cmd = event.data.cmd;
  const args = event.data;
  if (cmd === 'linkData') {
    if (args.rid) {  // We are editing a link, prepopulate the link data.
      document.title = editLinkTitle;
      $(IDS.DIALOG_TITLE).textContent = editLinkTitle;
      prepopulateFields(args.rid);
    } else {  // We are adding a link, disable the delete button.
      document.title = addLinkTitle;
      $(IDS.DIALOG_TITLE).textContent = addLinkTitle;
      $(IDS.DELETE).disabled = true;
      $(IDS.DONE).disabled = true;
      // Set accessibility names.
      $(IDS.DONE).setAttribute('aria-label', addLinkTitle);
      $(IDS.DONE).title = addLinkTitle;
    }
    // Timeout is required to allow the iframe to become visible before focusing
    // the first input field.
    window.setTimeout(() => {
      $(IDS.TITLE_FIELD).select();
    }, 10);
  }
}

/**
 * Does some initialization and shows the dialog window.
 */
function init() {
  // Parse query arguments.
  const query = window.location.search.substring(1).split('&');
  queryArgs = {};
  for (let i = 0; i < query.length; ++i) {
    const val = query[i].split('=');
    if (val[0] == '') {
      continue;
    }
    queryArgs[decodeURIComponent(val[0])] = decodeURIComponent(val[1]);
  }

  document.title = queryArgs['editTitle'];

  // Enable RTL.
  if (queryArgs['rtl'] == '1') {
    document.documentElement.setAttribute('dir', 'rtl');
  }

  // Populate text content.
  addLinkTitle = queryArgs['addTitle'];
  editLinkTitle = queryArgs['editTitle'];
  deleteLinkTitle = queryArgs['linkRemove'];
  $(IDS.DIALOG_TITLE).textContent = addLinkTitle;
  $(IDS.TITLE_FIELD_NAME).textContent = queryArgs['nameField'];
  $(IDS.TITLE_FIELD_NAME).setAttribute('aria-label', queryArgs['nameField']);
  $(IDS.URL_FIELD_NAME).textContent = queryArgs['urlField'];
  $(IDS.URL_FIELD_NAME).setAttribute('aria-label', queryArgs['urlField']);
  $(IDS.DELETE).textContent = $(IDS.DELETE).title = queryArgs['linkRemove'];
  $(IDS.CANCEL).textContent = $(IDS.CANCEL).title = queryArgs['linkCancel'];
  $(IDS.CANCEL).setAttribute('aria-label', queryArgs['linkCancel']);
  $(IDS.DONE).textContent = $(IDS.DONE).title = queryArgs['linkDone'];
  $(IDS.INVALID_URL).textContent = queryArgs['invalidUrl'];

  // Set up event listeners.
  document.body.onkeydown = (event) => {
    if (event.keyCode === KEYCODES.ESC) {
      // Close the iframe instead of just this dialog.
      event.preventDefault();
      closeDialog();
    }
  };
  $(IDS.DELETE).addEventListener('click', deleteLink);
  $(IDS.CANCEL).addEventListener('click', closeDialog);
  $(IDS.CANCEL).addEventListener('keydown', focusBackOnCancel);
  $(IDS.FORM).addEventListener('submit', (event) => {
    // Prevent the form from submitting and modifying the URL.
    event.preventDefault();
    finishEditLink();
  });
  const finishEditOrClose = (event) => {
    if (event.keyCode === KEYCODES.ENTER) {
      event.preventDefault();
      if (!$(IDS.DONE).disabled) {
        finishEditLink();
      }
    }
  };
  $(IDS.TITLE_FIELD).onkeydown = finishEditOrClose;
  $(IDS.URL_FIELD).onkeydown = finishEditOrClose;
  utils.disableOutlineOnMouseClick($(IDS.DELETE));
  utils.disableOutlineOnMouseClick($(IDS.CANCEL));
  utils.disableOutlineOnMouseClick($(IDS.DONE));

  animations.addRippleAnimations();

  // Change input field name to blue on input field focus.
  const changeColor = (fieldTitle) => {
    $(fieldTitle).classList.toggle('focused');
  };
  $(IDS.TITLE_FIELD)
      .addEventListener('focusin', () => changeColor(IDS.TITLE_FIELD_NAME));
  $(IDS.TITLE_FIELD)
      .addEventListener('blur', () => changeColor(IDS.TITLE_FIELD_NAME));
  $(IDS.URL_FIELD)
      .addEventListener('focusin', () => changeColor(IDS.URL_FIELD_NAME));
  $(IDS.URL_FIELD)
      .addEventListener('blur', () => changeColor(IDS.URL_FIELD_NAME));
  // Disables the "Done" button when the URL field is empty.
  $(IDS.URL_FIELD)
      .addEventListener(
          'input',
          () => $(IDS.DONE).disabled = ($(IDS.URL_FIELD).value.trim() === ''));

  utils.setPlatformClass(document.body);

  $(IDS.EDIT_DIALOG).showModal();

  window.addEventListener('message', handlePostMessage);
}

window.addEventListener('DOMContentLoaded', init);
