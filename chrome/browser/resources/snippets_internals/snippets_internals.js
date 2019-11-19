// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/** @type {snippetsInternals.mojom.PageHandlerRemote} */
let pageHandler = null;

/** @type {snippetsInternals.mojom.PageInterface} */
let page = null;

/* Javascript module for chrome://snippets-internals. */
(function() {

/* Utility functions*/

/**
 * Sets all the properties contained in the mapping in the page.
 * property map {id -> value}.
 * @param {!Object<string,string>} propertyMap Property name to value mapping.
 */
function setPropertiesInPage(propertyMap) {
  Object.keys(propertyMap).forEach(function(field) {
    setPropertyInPage(field, propertyMap[field]);
  });
}

/**
 * Sets the given value as textContent for the given field.
 * @param {string} field Id of the element to set the property on.
 * @param {string} value Property to be set in the page.
 */
function setPropertyInPage(field, value) {
  $(field).textContent = value;
}

/**
 * Downloads the given data under filename with the given datatype.
 * Acceptable values for data type include: text/plain, application/json, etc.
 * @param {string} fileName Name of the file to download.
 * @param {string} dataType The content-type to download.
 * @param {string} data The data to download.
 */
function downloadData(fileName, dataType, data) {
  let dataToReport = data;
  if (data === '') {
    dataToReport = 'None';
  }
  const link = document.createElement('a');
  link.download = fileName;
  link.href = 'data:' + dataType + ',' + encodeURI(dataToReport);
  link.click();
}

/**
 * Wrapper funciton for downloadData that stringifies json.
 * @param {string} fileName Name of file to download.
 * @param {string} data JSON data to download.
 */
function downloadJson(fileName, data) {
  downloadData(fileName, 'application/json', data);
}

/**
 * Clears children of the given domId.
 * @param {string} domId Id of the DOM element to be cleared.
 */
function clearChildrenForId(domId) {
  const parent = $(domId);
  while (parent.firstChild) {
    parent.removeChild(parent.firstChild);
  }
}

/* Page functions, as the elements appear of the page. */
function updateGeneralProperties() {
  pageHandler.getGeneralProperties().then(
      response => setPropertiesInPage(response.properties));
}

function getUserClassifierProperties() {
  pageHandler.getUserClassifierProperties().then(
      response => setPropertiesInPage(response.properties));
}

function getCategoryRankerProperties() {
  pageHandler.getCategoryRankerProperties().then(function(response) {
    const domId = 'category-ranker-table';
    clearChildrenForId(domId);

    const table = $(domId);
    const rowTemplate = $('category-ranker-row');
    Object.keys(response.properties).forEach(function(field) {
      const row = document.importNode(rowTemplate.content, true);
      const td = row.querySelectorAll('td');

      td[0].textContent = field;
      td[1].textContent = response.properties[field];
      table.appendChild(row);
    });
  });
}

/* Retrieve the remote content suggestions properties. */
function getRemoteContentSuggestionsProperties() {
  pageHandler.getRemoteContentSuggestionsProperties().then(function(response) {
    setPropertiesInPage(response.properties);
  });
}

/* Retrieve suggestions, ordered by category. */
function getSuggestionsByCategory() {
  pageHandler.getSuggestionsByCategory().then(function(response) {
    const domId = 'content-suggestions';
    const toggleClass = 'hidden-toggler';

    jstProcess(new JsEvalContext(response), $(domId));

    let text;
    let display;

    if (response.categories.length > 0) {
      text = '';
      display = 'inline';
    } else {
      text = 'The list is empty.';
      display = 'none';
    }

    const emptyNode = $(`${domId}-empty`);
    if (emptyNode) {
      emptyNode.textContent = text;
    }

    const clearNode = $(`${domId}-clear`);
    if (clearNode) {
      clearNode.style.display = display;
    }

    // Toggle visibility for suggestions.
    const links = document.getElementsByClassName(toggleClass);
    for (const link of links) {
      link.onclick = function(event) {
        const id = event.currentTarget.getAttribute('hidden-id');
        $(id).classList.toggle('hidden');
      };
    }

    // Clear dismissed suggestions.
    const clearDismissedButtons =
        document.getElementsByClassName('submit-clear-dismissed-suggestions');
    for (const button of clearDismissedButtons) {
      button.onclick = function(event) {
        // This is an attribute set on the elements.
        const id = parseInt(event.currentTarget.dataset.categoryId, 10);

        // Clear the suggestions and hide the table.
        pageHandler.clearDismissedSuggestions(id);
        const table = $('dismissed-category-' + id);
        table.classList.add('hidden');

        // Reload the data.
        getSuggestionsByCategory();
      };
    }

    // Toggle viewing dismissed suggestions.
    const toggleDismissedButtons =
        document.getElementsByClassName('toggle-dismissed-suggestions');
    for (const button of toggleDismissedButtons) {
      button.onclick = function(event) {
        // This is an attribute set on the elements.
        const id = parseInt(event.currentTarget.dataset.categoryId, 10);
        const table = $('dismissed-category-' + id);
        table.classList.toggle('hidden');
      };
    }
  });
}

/* Wrapper functions for setting up page. */

/* Refresh data. */
function refreshContent() {
  updateGeneralProperties();
  getUserClassifierProperties();
  getCategoryRankerProperties();
  getRemoteContentSuggestionsProperties();
}

/* Setup buttons and other event listeners. */
function setupEventListeners() {
  $('clear-classification').addEventListener('click', function(event) {
    pageHandler.clearUserClassifierProperties();
  });

  $('reload-suggestions').addEventListener('click', function(event) {
    pageHandler.reloadSuggestions();
  });

  $('clear-cached-suggestions').addEventListener('click', function(event) {
    pageHandler.clearCachedSuggestions();
  });

  $('background-fetch-button').addEventListener('click', function(event) {
    $('background-fetch-button').disabled = true;
    pageHandler.fetchSuggestionsInBackground(2).then(function(response) {
      $('background-fetch-button').disabled = false;
      $('last-json-container').classList.add('hidden');
      $('last-json-button').textContent = 'Show the last JSON';

      // After we've fetched, update the page.
      getRemoteContentSuggestionsProperties();
    });
  });

  $('last-json-button').addEventListener('click', function(event) {
    pageHandler.getLastJson().then(function(response) {
      const container = $('last-json-container');
      container.classList.toggle('hidden');

      $('last-json-text').textContent = response.json;
      $('last-json-button').textContent =
          container.classList.contains('hidden') ? 'Show the last JSON' :
                                                   'Hide the last JSON';
    });
  });

  $('last-json-dump').addEventListener('click', function(event) {
    pageHandler.getLastJson().then(function(response) {
      downloadJson('last_snippets.json', response.json);
    });
  });

  $('submit-dump').addEventListener('click', function(event) {
    pageHandler.getSuggestionsByCategory().then(function(response) {
      downloadJson('snippets.json', JSON.stringify(response.categories));
    });
  });

  window.addEventListener('focus', getSuggestionsByCategory);
}

/* Represents the js-side of the IPC link. Backend talks to this. */
/** @implements {snippetsInternals.mojom.PageInterface} */
class SnippetsInternalsPageImpl {
  constructor() {
    this.receiver_ = new snippetsInternals.mojom.PageReceiver(this);
  }

  /**
   * @return {!snippetsInternals.mojom.PageRemote}
   */
  bindNewPipeAndPassRemote() {
    return this.receiver_.$.bindNewPipeAndPassRemote();
  }

  /* Callback for when suggestions change on the backend. */
  onSuggestionsChanged() {
    getSuggestionsByCategory();
  }
}

/* Main entry point. */
document.addEventListener('DOMContentLoaded', function() {
  // Setup frontend mojo.
  page = new SnippetsInternalsPageImpl();

  // Setup backend mojo.
  const pageHandlerFactory =
      snippetsInternals.mojom.PageHandlerFactory.getRemote();

  // Give backend mojo a reference to frontend mojo.
  pageHandlerFactory.createPageHandler(page.bindNewPipeAndPassRemote())
      .then((response) => {
        pageHandler = response.handler;

        // Populate value fields.
        refreshContent();
        getSuggestionsByCategory();
        setInterval(refreshContent, 2000);

        // Setup events.
        setupEventListeners();
      });
});
}());
