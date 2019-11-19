// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Introduces users to ChromeVox.
 */

goog.provide('Tutorial');

goog.require('Msgs');
goog.require('AbstractEarcons');

/**
 * @constructor
 */
Tutorial = function() {
  /**
   * The 0-based index of the current page of the tutorial.
   * @type {number}
   * @private
   */
  this.page_;

  this.page = sessionStorage['tutorial_page_pos'] !== undefined ?
      sessionStorage['tutorial_page_pos'] :
      0;

  /** @private {boolean} */
  this.incognito_ = !!chrome.runtime.getManifest()['incognito'];
};

/**
 * @param {Node} container
 * @private
 */
Tutorial.buildEarconPage_ = function(container) {
  for (var earconId in EarconDescription) {
    var msgid = EarconDescription[earconId];
    var earconElement = document.createElement('p');
    earconElement.innerText = Msgs.getMsg(msgid);
    earconElement.setAttribute('tabindex', 0);
    var prevEarcon;
    var playEarcon = function(earcon) {
      if (prevEarcon) {
        chrome.extension
            .getBackgroundPage()['ChromeVox']['earcons']['cancelEarcon'](
                prevEarcon);
      }
      chrome.extension
          .getBackgroundPage()['ChromeVox']['earcons']['playEarcon'](earcon);
      prevEarcon = earcon;
    }.bind(this, earconId);
    earconElement.addEventListener('focus', playEarcon, false);
    container.appendChild(earconElement);
  }
};

/**
 * Data for the ChromeVox tutorial consisting of a list of pages,
 * each one of which contains a list of objects, where each object has
 * a message ID for some text, and optional attributes indicating if it's
 * a heading, link, text for only one platform, etc.
 *
 * @type Array<Array<Object>>
 */
Tutorial.PAGES = [
  [
    {msgid: 'tutorial_welcome_heading', heading: true},
    {msgid: 'tutorial_welcome_text'},
    {msgid: 'tutorial_enter_to_advance', repeat: true},
  ],
  [
    {msgid: 'tutorial_on_off_heading', heading: true},
    {msgid: 'tutorial_control'},
    {msgid: 'tutorial_on_off'},
    {msgid: 'tutorial_enter_to_advance', repeat: true},
  ],
  [
    {msgid: 'tutorial_modifier_heading', heading: true},
    {msgid: 'tutorial_modifier'},
    {msgid: 'tutorial_chromebook_search', chromebookOnly: true},
    {msgid: 'tutorial_any_key'},
    {msgid: 'tutorial_enter_to_advance', repeat: true},
  ],
  [
    {msgid: 'tutorial_basic_navigation_heading', heading: true},
    {msgid: 'tutorial_basic_navigation'},
    {msgid: 'tutorial_click_next'},
  ],
  [
    {msgid: 'tutorial_jump_heading', heading: true},
    {msgid: 'tutorial_jump'},
    {msgid: 'tutorial_jump_second_heading', heading: true},
    {msgid: 'tutorial_jump_wrap_heading', heading: true},
    {msgid: 'tutorial_click_next'},
  ],
  [
    {msgid: 'tutorial_menus_heading', heading: true},
    {msgid: 'tutorial_menus'},
    {msgid: 'tutorial_click_next'},
  ],
  [
    {msgid: 'tutorial_chrome_shortcuts_heading', heading: true},
    {msgid: 'tutorial_chrome_shortcuts'},
    {msgid: 'tutorial_chromebook_ctrl_forward', chromebookOnly: true}
  ],
  [
    {msgid: 'tutorial_earcon_page_title', heading: true},
    {msgid: 'tutorial_earcon_page_body'}, {custom: Tutorial.buildEarconPage_}
  ],
  [
    {msgid: 'tutorial_touch_heading', heading: true},
    {msgid: 'tutorial_touch_intro'}, {
      list: true,
      items: [
        {msgid: 'tutorial_touch_drag_one_finger', listItem: true},
        {msgid: 'tutorial_touch_swipe_left_right', listItem: true},
        {msgid: 'tutorial_touch_swipe_up_down', listItem: true},
        {msgid: 'tutorial_touch_double_tap', listItem: true},
        {msgid: 'tutorial_touch_four_finger_tap', listItem: true},
        {msgid: 'tutorial_touch_two_finger_tap', listItem: true},
      ]
    },
    {msgid: 'tutorial_touch_learn_more'}
  ],
  [
    {msgid: 'tutorial_learn_more_heading', heading: true},
    {msgid: 'tutorial_learn_more'},
    {
      msgid: 'next_command_reference',
      link: 'http://www.chromevox.com/next_keyboard_shortcuts.html'
    },
    {
      msgid: 'chrome_keyboard_shortcuts',
      link: 'https://support.google.com/chromebook/answer/183101?hl=en'
    },
    {
      msgid: 'touchscreen_accessibility',
      link: 'https://support.google.com/chromebook/answer/6103702?hl=en'
    },
  ],
];

Tutorial.prototype = {
  /**
   * Handles key down events.
   * @param {Event} evt
   * @return {boolean}
   */
  onKeyDown: function(evt) {
    if (document.activeElement &&
        (document.activeElement.id == 'tutorial_previous' ||
         document.activeElement.id == 'tutorial_next'))
      return true;

    if (evt.key == 'Enter') {
      this.nextPage();
    } else if (evt.key == 'Backspace') {
      this.previousPage();
    } else {
      return true;
    }
    return false;
  },

  /** Open the last viewed page in the tutorial. */
  lastViewedPage: function() {
    this.page = sessionStorage['tutorial_page_pos'] !== undefined ?
        sessionStorage['tutorial_page_pos'] :
        0;
    if (this.page == -1) {
      this.page = 0;
    }
    this.showCurrentPage_();
  },

  /** Open the update notes page. */
  updateNotes: function() {
    delete sessionStorage['tutorial_page_pos'];
    this.page = -1;
    this.showPage_([
      {msgid: 'update_63_title', heading: true},
      {msgid: 'update_63_intro'},
      {
        list: true,
        items: [
          {msgid: 'update_63_item_1', listItem: true},
          {msgid: 'update_63_item_2', listItem: true},
          {msgid: 'update_63_item_3', listItem: true},
        ],
      },
      {msgid: 'update_63_OUTTRO'},
    ]);
  },

  /** Move to the next page in the tutorial. */
  nextPage: function() {
    if (this.page < Tutorial.PAGES.length - 1) {
      this.page++;
      this.showCurrentPage_();
    }
  },

  /** Move to the previous page in the tutorial. */
  previousPage: function() {
    if (this.page > 0) {
      this.page--;
      this.showCurrentPage_();
    }
  },

  /**
   * Shows the page for page |this.page_|.
   */
  showCurrentPage_: function() {
    var pageElements = Tutorial.PAGES[this.page] || [];
    this.showPage_(pageElements);
  },

  /**
   * Recreate the tutorial DOM using |pageElements|.
   * @param {!Array<Object>} pageElements
   * @private
   */
  showPage_: function(pageElements) {
    var tutorialContainer = $('tutorial_main');
    tutorialContainer.innerHTML = '';
    this.buildDom_(pageElements, tutorialContainer);
    this.finalizeDom_();
  },

  /**
   * Builds a dom under the container
   * @param {!Array<Object>} pageElements
   * @param {!Node} container
   * @private
   */
  buildDom_: function(pageElements, container) {
    var focus;
    for (var i = 0; i < pageElements.length; ++i) {
      var pageElement = pageElements[i];
      var msgid = pageElement.msgid;
      var text = '';
      if (msgid) {
        text = Msgs.getMsg(msgid);
      }
      var element;
      if (pageElement.heading) {
        element = document.createElement('h2');
        element.setAttribute('tabindex', -1);
        if (!focus) {
          focus = element;
        }
      } else if (pageElement.list) {
        element = document.createElement('ul');
        this.buildDom_(pageElement.items, element);
      } else if (pageElement.listItem) {
        element = document.createElement('li');
      } else if (pageElement.link) {
        element = document.createElement('a');
        element.href = pageElement.link;
        if (!this.incognito_) {
          element.setAttribute('tabindex', 0);
        } else {
          element.disabled = true;
        }
        element.addEventListener('click', function(evt) {
          if (this.incognito_) {
            return;
          }

          Panel.closeMenusAndRestoreFocus();
          chrome.windows.create({url: evt.target.href});
          return false;
        }.bind(this), false);
      } else if (pageElement.custom) {
        element = document.createElement('div');
        pageElement.custom(element);
      } else {
        element = document.createElement('p');
      }
      if (text) {
        element.innerText = text;
      }
      container.appendChild(element);
    }
    if (focus) {
      focus.focus();
    }
  },

  /** @private */
  finalizeDom_: function() {
    var disableNext = this.page == (Tutorial.PAGES.length - 1);
    var disablePrevious = this.page == 0;
    $('tutorial_next').setAttribute('aria-disabled', disableNext);
    $('tutorial_previous').setAttribute('aria-disabled', disablePrevious);
  },

  get page() {
    return this.page_;
  },

  set page(val) {
    this.page_ = val;
    sessionStorage['tutorial_page_pos'] = this.page_;
  }
};
