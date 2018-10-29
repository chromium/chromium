// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview ChromeVox commands.
 */

goog.provide('CommandHandler');

goog.require('ChromeVoxState');
goog.require('CustomAutomationEvent');
goog.require('LogStore');
goog.require('Output');
goog.require('TreeDumper');
goog.require('cvox.ChromeVoxBackground');
goog.require('cvox.ChromeVoxPrefs');
goog.require('cvox.ChromeVoxKbHandler');

goog.scope(function() {
var AutomationEvent = chrome.automation.AutomationEvent;
var AutomationNode = chrome.automation.AutomationNode;
var Dir = constants.Dir;
var EventType = chrome.automation.EventType;
var RoleType = chrome.automation.RoleType;
var StateType = chrome.automation.StateType;

/**
 * Handles ChromeVox Next commands.
 * @param {string} command
 * @return {boolean} True if the command should propagate.
 */
CommandHandler.onCommand = function(command) {
  // Check for loss of focus which results in us invalidating our current
  // range. Note this call is synchronis.
  chrome.automation.getFocus(function(focusedNode) {
    var cur = ChromeVoxState.instance.currentRange;
    if (cur && !cur.isValid()) {
      ChromeVoxState.instance.setCurrentRange(
          cursors.Range.fromNode(focusedNode));
    }

    if (!focusedNode)
      ChromeVoxState.instance.setCurrentRange(null);
  });

  // These commands don't require a current range.
  switch (command) {
    case 'speakTimeAndDate':
      chrome.automation.getDesktop(function(d) {
        // First, try speaking the on-screen time.
        var allTime = d.findAll({role: RoleType.TIME});
        allTime.filter(function(t) {
          return t.root.role == RoleType.DESKTOP;
        });

        var timeString = '';
        allTime.forEach(function(t) {
          if (t.name)
            timeString = t.name;
        });
        if (timeString) {
          cvox.ChromeVox.tts.speak(timeString, cvox.QueueMode.FLUSH);
        } else {
          // Fallback to the old way of speaking time.
          var output = new Output();
          var dateTime = new Date();
          output
              .withString(
                  dateTime.toLocaleTimeString() + ', ' +
                  dateTime.toLocaleDateString())
              .go();
        }
      });
      return false;
    case 'showOptionsPage':
      chrome.runtime.openOptionsPage();
      break;
    case 'toggleChromeVox':
      if (cvox.ChromeVox.isChromeOS)
        return false;

      cvox.ChromeVox.isActive = !cvox.ChromeVox.isActive;
      if (!cvox.ChromeVox.isActive) {
        var msg = Msgs.getMsg('chromevox_inactive');
        cvox.ChromeVox.tts.speak(msg, cvox.QueueMode.FLUSH);
        return false;
      }
      break;
    case 'toggleStickyMode':
      cvox.ChromeVoxBackground.setPref(
          'sticky', !cvox.ChromeVox.isStickyPrefOn, true);

      if (cvox.ChromeVox.isStickyPrefOn)
        chrome.accessibilityPrivate.setKeyboardListener(true, true);
      else
        chrome.accessibilityPrivate.setKeyboardListener(true, false);
      return false;
    case 'passThroughMode':
      cvox.ChromeVox.passThroughMode = true;
      cvox.ChromeVox.tts.speak(
          Msgs.getMsg('pass_through_key'), cvox.QueueMode.QUEUE);
      return true;
    case 'showKbExplorerPage':
      var explorerPage = {
        url: 'chromevox/background/kbexplorer.html',
        type: 'panel'
      };
      chrome.windows.create(explorerPage);
      break;
    case 'showLogPage':
      chrome.commandLinePrivate.hasSwitch(
          'enable-chromevox-developer-option', function(enable) {
            if (enable) {
              var logPage = {url: 'cvox2/background/log.html'};
              chrome.tabs.create(logPage);
            }
          });
      break;
    case 'enableLogging':
      chrome.commandLinePrivate.hasSwitch(
          'enable-chromevox-developer-option', function(enable) {
            if (enable) {
              var prefs = new cvox.ChromeVoxPrefs();
              for (var type in cvox.ChromeVoxPrefs.loggingPrefs) {
                prefs.setLoggingPrefs(
                    cvox.ChromeVoxPrefs.loggingPrefs[type], true);
              }
            }
          });
      break;
    case 'disableLogging':
      chrome.commandLinePrivate.hasSwitch(
          'enable-chromevox-developer-option', function(enable) {
            if (enable) {
              var prefs = new cvox.ChromeVoxPrefs();
              for (var type in cvox.ChromeVoxPrefs.loggingPrefs) {
                prefs.setLoggingPrefs(
                    cvox.ChromeVoxPrefs.loggingPrefs[type], false);
              }
            }
          });
      break;
    case 'dumpTree':
      chrome.commandLinePrivate.hasSwitch(
          'enable-chromevox-developer-option', function(enable) {
            if (enable) {
              chrome.automation.getDesktop(function(root) {
                LogStore.getInstance().writeTreeLog(new TreeDumper(root));
              });
            }
          });
      break;
    case 'decreaseTtsRate':
      CommandHandler.increaseOrDecreaseSpeechProperty_(
          cvox.AbstractTts.RATE, false);
      return false;
    case 'increaseTtsRate':
      CommandHandler.increaseOrDecreaseSpeechProperty_(
          cvox.AbstractTts.RATE, true);
      return false;
    case 'decreaseTtsPitch':
      CommandHandler.increaseOrDecreaseSpeechProperty_(
          cvox.AbstractTts.PITCH, false);
      return false;
    case 'increaseTtsPitch':
      CommandHandler.increaseOrDecreaseSpeechProperty_(
          cvox.AbstractTts.PITCH, true);
      return false;
    case 'decreaseTtsVolume':
      CommandHandler.increaseOrDecreaseSpeechProperty_(
          cvox.AbstractTts.VOLUME, false);
      return false;
    case 'increaseTtsVolume':
      CommandHandler.increaseOrDecreaseSpeechProperty_(
          cvox.AbstractTts.VOLUME, true);
      return false;
    case 'stopSpeech':
      cvox.ChromeVox.tts.stop();
      ChromeVoxState.isReadingContinuously = false;
      return false;
    case 'toggleEarcons':
      cvox.AbstractEarcons.enabled = !cvox.AbstractEarcons.enabled;
      var announce = cvox.AbstractEarcons.enabled ? Msgs.getMsg('earcons_on') :
                                                    Msgs.getMsg('earcons_off');
      cvox.ChromeVox.tts.speak(
          announce, cvox.QueueMode.FLUSH,
          cvox.AbstractTts.PERSONALITY_ANNOTATION);
      return false;
    case 'cycleTypingEcho':
      cvox.ChromeVox.typingEcho =
          cvox.TypingEcho.cycle(cvox.ChromeVox.typingEcho);
      var announce = '';
      switch (cvox.ChromeVox.typingEcho) {
        case cvox.TypingEcho.CHARACTER:
          announce = Msgs.getMsg('character_echo');
          break;
        case cvox.TypingEcho.WORD:
          announce = Msgs.getMsg('word_echo');
          break;
        case cvox.TypingEcho.CHARACTER_AND_WORD:
          announce = Msgs.getMsg('character_and_word_echo');
          break;
        case cvox.TypingEcho.NONE:
          announce = Msgs.getMsg('none_echo');
          break;
      }
      cvox.ChromeVox.tts.speak(
          announce, cvox.QueueMode.FLUSH,
          cvox.AbstractTts.PERSONALITY_ANNOTATION);
      return false;
    case 'cyclePunctuationEcho':
      cvox.ChromeVox.tts.speak(
          Msgs.getMsg(ChromeVoxState.backgroundTts.cyclePunctuationEcho()),
          cvox.QueueMode.FLUSH);
      return false;
    case 'reportIssue':
      var url = 'https://code.google.com/p/chromium/issues/entry?' +
          'labels=Type-Bug,Pri-2,cvox2,OS-Chrome&' +
          'components=UI>accessibility&' +
          'description=';

      var description = {};
      description['Version'] = chrome.app.getDetails().version;
      description['Reproduction Steps'] = '%0a1.%0a2.%0a3.';
      for (var key in description)
        url += key + ':%20' + description[key] + '%0a';
      chrome.tabs.create({url: url});
      return false;
    case 'toggleBrailleCaptions':
      cvox.BrailleCaptionsBackground.setActive(
          !cvox.BrailleCaptionsBackground.isEnabled());
      return false;
    case 'toggleBrailleTable':
      var brailleTableType = localStorage['brailleTableType'];
      var output = '';
      if (brailleTableType == 'brailleTable6') {
        brailleTableType = 'brailleTable8';

        // This label reads "switch to 8 dot braille".
        output = '@OPTIONS_BRAILLE_TABLE_TYPE_6';
      } else {
        brailleTableType = 'brailleTable6';

        // This label reads "switch to 6 dot braille".
        output = '@OPTIONS_BRAILLE_TABLE_TYPE_8';
      }

      localStorage['brailleTable'] = localStorage[brailleTableType];
      localStorage['brailleTableType'] = brailleTableType;
      cvox.BrailleBackground.getInstance().getTranslatorManager().refresh(
          localStorage[brailleTableType]);
      new Output().format(output).go();
      return false;
    case 'help':
      (new PanelCommand(PanelCommandType.TUTORIAL)).send();
      return false;
    case 'showNextUpdatePage':
      (new PanelCommand(PanelCommandType.UPDATE_NOTES)).send();
      localStorage['notifications_update_notification_shown'] = true;
      return false;
    case 'darkenScreen':
      chrome.accessibilityPrivate.darkenScreen(true);
      new Output().format('@darken_screen').go();
      return false;
    case 'undarkenScreen':
      chrome.accessibilityPrivate.darkenScreen(false);
      new Output().format('@undarken_screen').go();
      return false;
    case 'toggleSpeechOnOrOff':
      var state = cvox.ChromeVox.tts.toggleSpeechOnOrOff();
      new Output().format(state ? '@speech_on' : '@speech_off').go();
      return false;
    case 'enableChromeVoxArcSupportForCurrentApp':
      chrome.accessibilityPrivate.setNativeChromeVoxArcSupportForCurrentApp(
          true);
      break;
    case 'disableChromeVoxArcSupportForCurrentApp':
      chrome.accessibilityPrivate.setNativeChromeVoxArcSupportForCurrentApp(
          false);
      break;
    case 'showTtsSettings':
      var ttsSettings = {url: 'chrome://settings/manageAccessibility/tts'};
      chrome.windows.create(ttsSettings);
      break;
    default:
      break;
  }

  // Require a current range.
  if (!ChromeVoxState.instance.currentRange_)
    return true;

  var current = ChromeVoxState.instance.currentRange;

  // If true, will check if the predicate matches the current node.
  var matchCurrent = false;

  // Allow edit commands first.
  if (!CommandHandler.onEditCommand_(command))
    return false;

  var dir = Dir.FORWARD;
  var pred = null;
  var predErrorMsg = undefined;
  var rootPred = AutomationPredicate.rootOrEditableRoot;
  var shouldWrap = true;
  var speechProps = {};
  var skipSync = false;
  var didNavigate = false;
  var tryScrolling = true;
  switch (command) {
    case 'nextCharacter':
      didNavigate = true;
      speechProps['phoneticCharacters'] = true;
      current = current.move(cursors.Unit.CHARACTER, Dir.FORWARD);
      break;
    case 'previousCharacter':
      dir = Dir.BACKWARD;
      didNavigate = true;
      speechProps['phoneticCharacters'] = true;
      current = current.move(cursors.Unit.CHARACTER, dir);
      break;
    case 'nextWord':
      didNavigate = true;
      current = current.move(cursors.Unit.WORD, Dir.FORWARD);
      break;
    case 'previousWord':
      dir = Dir.BACKWARD;
      didNavigate = true;
      current = current.move(cursors.Unit.WORD, dir);
      break;
    case 'forward':
    case 'nextLine':
      didNavigate = true;
      current = current.move(cursors.Unit.LINE, Dir.FORWARD);
      break;
    case 'backward':
    case 'previousLine':
      dir = Dir.BACKWARD;
      didNavigate = true;
      current = current.move(cursors.Unit.LINE, dir);
      break;
    case 'nextButton':
      dir = Dir.FORWARD;
      pred = AutomationPredicate.button;
      predErrorMsg = 'no_next_button';
      break;
    case 'previousButton':
      dir = Dir.BACKWARD;
      pred = AutomationPredicate.button;
      predErrorMsg = 'no_previous_button';
      break;
    case 'nextCheckbox':
      pred = AutomationPredicate.checkBox;
      predErrorMsg = 'no_next_checkbox';
      break;
    case 'previousCheckbox':
      dir = Dir.BACKWARD;
      pred = AutomationPredicate.checkBox;
      predErrorMsg = 'no_previous_checkbox';
      break;
    case 'nextComboBox':
      pred = AutomationPredicate.comboBox;
      predErrorMsg = 'no_next_combo_box';
      break;
    case 'previousComboBox':
      dir = Dir.BACKWARD;
      pred = AutomationPredicate.comboBox;
      predErrorMsg = 'no_previous_combo_box';
      break;
    case 'nextEditText':
      pred = AutomationPredicate.editText;
      predErrorMsg = 'no_next_edit_text';
      break;
    case 'previousEditText':
      dir = Dir.BACKWARD;
      pred = AutomationPredicate.editText;
      predErrorMsg = 'no_previous_edit_text';
      break;
    case 'nextFormField':
      pred = AutomationPredicate.formField;
      predErrorMsg = 'no_next_form_field';
      break;
    case 'previousFormField':
      dir = Dir.BACKWARD;
      pred = AutomationPredicate.formField;
      predErrorMsg = 'no_previous_form_field';
      break;
    case 'previousGraphic':
      dir = Dir.BACKWARD;
      pred = AutomationPredicate.image;
      predErrorMsg = 'no_previous_graphic';
      break;
    case 'nextGraphic':
      pred = AutomationPredicate.image;
      predErrorMsg = 'no_next_graphic';
      break;
    case 'nextHeading':
      pred = AutomationPredicate.heading;
      predErrorMsg = 'no_next_heading';
      break;
    case 'nextHeading1':
      pred = AutomationPredicate.makeHeadingPredicate(1);
      predErrorMsg = 'no_next_heading_1';
      break;
    case 'nextHeading2':
      pred = AutomationPredicate.makeHeadingPredicate(2);
      predErrorMsg = 'no_next_heading_2';
      break;
    case 'nextHeading3':
      pred = AutomationPredicate.makeHeadingPredicate(3);
      predErrorMsg = 'no_next_heading_3';
      break;
    case 'nextHeading4':
      pred = AutomationPredicate.makeHeadingPredicate(4);
      predErrorMsg = 'no_next_heading_4';
      break;
    case 'nextHeading5':
      pred = AutomationPredicate.makeHeadingPredicate(5);
      predErrorMsg = 'no_next_heading_5';
      break;
    case 'nextHeading6':
      pred = AutomationPredicate.makeHeadingPredicate(6);
      predErrorMsg = 'no_next_heading_6';
      break;
    case 'previousHeading':
      dir = Dir.BACKWARD;
      pred = AutomationPredicate.heading;
      predErrorMsg = 'no_previous_heading';
      break;
    case 'previousHeading1':
      dir = Dir.BACKWARD;
      pred = AutomationPredicate.makeHeadingPredicate(1);
      predErrorMsg = 'no_previous_heading_1';
      break;
    case 'previousHeading2':
      dir = Dir.BACKWARD;
      pred = AutomationPredicate.makeHeadingPredicate(2);
      predErrorMsg = 'no_previous_heading_2';
      break;
    case 'previousHeading3':
      dir = Dir.BACKWARD;
      pred = AutomationPredicate.makeHeadingPredicate(3);
      predErrorMsg = 'no_previous_heading_3';
      break;
    case 'previousHeading4':
      dir = Dir.BACKWARD;
      pred = AutomationPredicate.makeHeadingPredicate(4);
      predErrorMsg = 'no_previous_heading_4';
      break;
    case 'previousHeading5':
      dir = Dir.BACKWARD;
      pred = AutomationPredicate.makeHeadingPredicate(5);
      predErrorMsg = 'no_previous_heading_5';
      break;
    case 'previousHeading6':
      dir = Dir.BACKWARD;
      pred = AutomationPredicate.makeHeadingPredicate(6);
      predErrorMsg = 'no_previous_heading_6';
      break;
    case 'nextLink':
      pred = AutomationPredicate.link;
      predErrorMsg = 'no_next_link';
      break;
    case 'previousLink':
      dir = Dir.BACKWARD;
      pred = AutomationPredicate.link;
      predErrorMsg = 'no_previous_link';
      break;
    case 'nextTable':
      pred = AutomationPredicate.table;
      predErrorMsg = 'no_next_table';
      break;
    case 'previousTable':
      dir = Dir.BACKWARD;
      pred = AutomationPredicate.table;
      predErrorMsg = 'no_previous_table';
      break;
    case 'nextVisitedLink':
      pred = AutomationPredicate.visitedLink;
      predErrorMsg = 'no_next_visited_link';
      break;
    case 'previousVisitedLink':
      dir = Dir.BACKWARD;
      pred = AutomationPredicate.visitedLink;
      predErrorMsg = 'no_previous_visited_link';
      break;
    case 'nextLandmark':
      pred = AutomationPredicate.landmark;
      predErrorMsg = 'no_next_landmark';
      break;
    case 'previousLandmark':
      dir = Dir.BACKWARD;
      pred = AutomationPredicate.landmark;
      predErrorMsg = 'no_previous_landmark';
      break;
    case 'right':
    case 'nextObject':
      didNavigate = true;
      current = current.move(cursors.Unit.NODE, Dir.FORWARD);
      break;
    case 'left':
    case 'previousObject':
      dir = Dir.BACKWARD;
      didNavigate = true;
      current = current.move(cursors.Unit.NODE, dir);
      break;
    case 'previousGroup':
      skipSync = true;
      dir = Dir.BACKWARD;
      pred = AutomationPredicate.group;
      break;
    case 'nextGroup':
      skipSync = true;
      pred = AutomationPredicate.group;
      break;
    case 'jumpToTop':
      var node = AutomationUtil.findNodePost(
          current.start.node.root, Dir.FORWARD, AutomationPredicate.object);
      if (node)
        current = cursors.Range.fromNode(node);
      tryScrolling = false;
      break;
    case 'jumpToBottom':
      var node = AutomationUtil.findLastNode(
          current.start.node.root, AutomationPredicate.object);
      if (node)
        current = cursors.Range.fromNode(node);
      tryScrolling = false;
      break;
    case 'forceClickOnCurrentItem':
      if (ChromeVoxState.instance.currentRange) {
        var actionNode = ChromeVoxState.instance.currentRange.start.node;
        // Scan for a clickable, which overrides the |actionNode|.
        var clickable = actionNode;
        while (clickable && !clickable.clickable)
          clickable = clickable.parent;
        if (clickable) {
          clickable.doDefault();
          return false;
        }

        if (EventSourceState.get() == EventSourceType.TOUCH_GESTURE &&
            AutomationPredicate.editText(actionNode)) {
          // Dispatch a click to ensure the VK gets shown.
          var location = actionNode.location;
          var event = {
            type: chrome.accessibilityPrivate.SyntheticMouseEventType.PRESS,
            x: location.left + Math.round(location.width / 2),
            y: location.top + Math.round(location.height / 2)
          };
          chrome.accessibilityPrivate.sendSyntheticMouseEvent(event);
          event.type =
              chrome.accessibilityPrivate.SyntheticMouseEventType.RELEASE;
          chrome.accessibilityPrivate.sendSyntheticMouseEvent(event);
          return false;
        }

        while (actionNode.role == RoleType.INLINE_TEXT_BOX ||
               actionNode.role == RoleType.STATIC_TEXT)
          actionNode = actionNode.parent;
        if (actionNode.inPageLinkTarget) {
          ChromeVoxState.instance.navigateToRange(
              cursors.Range.fromNode(actionNode.inPageLinkTarget));
        } else {
          actionNode.doDefault();
        }
      }
      // Skip all other processing; if focus changes, we should get an event
      // for that.
      return false;
    case 'jumpToDetails':
      var node = current.start.node;
      while (node && !node.details)
        node = node.parent;
      if (node)
        current = cursors.Range.fromNode(node.details);
      break;
    case 'readFromHere':
      ChromeVoxState.isReadingContinuously = true;
      var continueReading = function() {
        if (!ChromeVoxState.isReadingContinuously ||
            !ChromeVoxState.instance.currentRange)
          return;

        var prevRange = ChromeVoxState.instance.currentRange;
        var newRange = ChromeVoxState.instance.currentRange.move(
            cursors.Unit.NODE, Dir.FORWARD);

        // Stop if we've wrapped back to the document.
        var maybeDoc = newRange.start.node;
        if (AutomationPredicate.root(maybeDoc)) {
          ChromeVoxState.isReadingContinuously = false;
          return;
        }

        ChromeVoxState.instance.setCurrentRange(newRange);
        newRange.select();

        new Output()
            .withoutHints()
            .withRichSpeechAndBraille(
                ChromeVoxState.instance.currentRange, prevRange,
                Output.EventType.NAVIGATE)
            .onSpeechEnd(continueReading)
            .go();
      }.bind(this);
      var startNode = ChromeVoxState.instance.currentRange.start.node;
      var collapsedRange = cursors.Range.fromNode(startNode);
      new Output()
          .withoutHints()
          .withRichSpeechAndBraille(
              collapsedRange, collapsedRange, Output.EventType.NAVIGATE)
          .onSpeechEnd(continueReading)
          .go();

      return false;
    case 'contextMenu':
      if (ChromeVoxState.instance.currentRange) {
        var actionNode = ChromeVoxState.instance.currentRange.start.node;
        if (actionNode.role == RoleType.INLINE_TEXT_BOX)
          actionNode = actionNode.parent;
        actionNode.showContextMenu();
        return false;
      }
      break;
    case 'toggleKeyboardHelp':
      (new PanelCommand(PanelCommandType.OPEN_MENUS)).send();
      return false;
    case 'showPanelMenuMostRecent':
      (new PanelCommand(PanelCommandType.OPEN_MENUS_MOST_RECENT)).send();
      return false;
    case 'showHeadingsList':
      (new PanelCommand(PanelCommandType.OPEN_MENUS, 'role_heading')).send();
      return false;
    case 'showFormsList':
      (new PanelCommand(PanelCommandType.OPEN_MENUS, 'role_form')).send();
      return false;
    case 'showLandmarksList':
      (new PanelCommand(PanelCommandType.OPEN_MENUS, 'role_landmark')).send();
      return false;
    case 'showLinksList':
      (new PanelCommand(PanelCommandType.OPEN_MENUS, 'role_link')).send();
      return false;
    case 'showTablesList':
      (new PanelCommand(PanelCommandType.OPEN_MENUS, 'table_strategy')).send();
      return false;
    case 'toggleSearchWidget':
      (new PanelCommand(PanelCommandType.SEARCH)).send();
      return false;
    case 'readCurrentTitle':
      var target = ChromeVoxState.instance.currentRange.start.node;
      var output = new Output();
      target = AutomationUtil.getTopLevelRoot(target) || target.parent;

      // Search for a container (e.g. rootWebArea, window) with a title-like
      // string.
      while (target && !target.name && !target.docUrl)
        target = target.parent;

      if (!target)
        output.format('@no_title');
      else
        output.withString(target.name || target.docUrl);

      output.go();
      return false;
    case 'readCurrentURL':
      var output = new Output();
      var target = ChromeVoxState.instance.currentRange.start.node.root;
      output.withString(target.docUrl || '').go();
      return false;
    case 'toggleSelection':
      if (!ChromeVoxState.instance.pageSel_) {
        ChromeVoxState.instance.pageSel_ = ChromeVoxState.instance.currentRange;
        DesktopAutomationHandler.instance.ignoreDocumentSelectionFromAction(
            true);
      } else {
        var root = ChromeVoxState.instance.currentRange.start.node.root;
        if (root && root.anchorObject && root.focusObject) {
          var sel = new cursors.Range(
              new cursors.Cursor(root.anchorObject, root.anchorOffset),
              new cursors.Cursor(root.focusObject, root.focusOffset));
          var o = new Output()
                      .format('@end_selection')
                      .withSpeechAndBraille(sel, sel, Output.EventType.NAVIGATE)
                      .go();
          DesktopAutomationHandler.instance.ignoreDocumentSelectionFromAction(
              false);
        }
        ChromeVoxState.instance.pageSel_ = null;
        return false;
      }
      break;
    case 'fullyDescribe':
      var o = new Output();
      o.withContextFirst()
          .withRichSpeechAndBraille(current, null, Output.EventType.NAVIGATE)
          .go();
      return false;
    case 'viewGraphicAsBraille':
      CommandHandler.viewGraphicAsBraille_(current);
      return false;
    // Table commands.
    case 'previousRow':
      dir = Dir.BACKWARD;
      var tableOpts = {row: true, dir: dir};
      pred = AutomationPredicate.makeTableCellPredicate(
          current.start.node, tableOpts);
      predErrorMsg = 'no_cell_above';
      rootPred = AutomationPredicate.table;
      shouldWrap = false;
      break;
    case 'previousCol':
      dir = Dir.BACKWARD;
      var tableOpts = {col: true, dir: dir};
      pred = AutomationPredicate.makeTableCellPredicate(
          current.start.node, tableOpts);
      predErrorMsg = 'no_cell_left';
      rootPred = AutomationPredicate.row;
      shouldWrap = false;
      break;
    case 'nextRow':
      var tableOpts = {row: true, dir: dir};
      pred = AutomationPredicate.makeTableCellPredicate(
          current.start.node, tableOpts);
      predErrorMsg = 'no_cell_below';
      rootPred = AutomationPredicate.table;
      shouldWrap = false;
      break;
    case 'nextCol':
      var tableOpts = {col: true, dir: dir};
      pred = AutomationPredicate.makeTableCellPredicate(
          current.start.node, tableOpts);
      predErrorMsg = 'no_cell_right';
      rootPred = AutomationPredicate.row;
      shouldWrap = false;
      break;
    case 'goToRowFirstCell':
    case 'goToRowLastCell':
      var node = current.start.node;
      while (node && node.role != RoleType.ROW)
        node = node.parent;
      if (!node)
        break;
      var end = AutomationUtil.findNodePost(
          node, command == 'goToRowLastCell' ? Dir.BACKWARD : Dir.FORWARD,
          AutomationPredicate.leaf);
      if (end)
        current = cursors.Range.fromNode(end);
      break;
    case 'goToColFirstCell':
      var node = current.start.node;
      while (node && node.role != RoleType.TABLE)
        node = node.parent;
      if (!node || !node.firstChild)
        return false;
      var tableOpts = {col: true, dir: dir, end: true};
      pred = AutomationPredicate.makeTableCellPredicate(
          current.start.node, tableOpts);
      current = cursors.Range.fromNode(node.firstChild);
      // Should not be outputted.
      predErrorMsg = 'no_cell_above';
      rootPred = AutomationPredicate.table;
      shouldWrap = false;
      break;
    case 'goToColLastCell':
      dir = Dir.BACKWARD;
      var node = current.start.node;
      while (node && node.role != RoleType.TABLE)
        node = node.parent;
      if (!node || !node.lastChild)
        return false;
      var tableOpts = {col: true, dir: dir, end: true};
      pred = AutomationPredicate.makeTableCellPredicate(
          current.start.node, tableOpts);

      // Try to start on the last cell of the table and allow
      // matching that node.
      var startNode = node.lastChild;
      while (startNode.lastChild &&
             !AutomationPredicate.cellLike(startNode.role))
        startNode = startNode.lastChild;
      current = cursors.Range.fromNode(startNode);
      matchCurrent = true;

      // Should not be outputted.
      predErrorMsg = 'no_cell_below';
      rootPred = AutomationPredicate.table;
      shouldWrap = false;
      break;
    case 'goToFirstCell':
    case 'goToLastCell':
      node = current.start.node;
      while (node && node.role != RoleType.TABLE)
        node = node.parent;
      if (!node)
        break;
      var end = AutomationUtil.findNodePost(
          node, command == 'goToLastCell' ? Dir.BACKWARD : Dir.FORWARD,
          AutomationPredicate.leaf);
      if (end)
        current = cursors.Range.fromNode(end);
      break;
    case 'scrollBackward':
      var node = current.start.node;
      while (node &&
             !node.standardActions.includes(
                 chrome.automation.ActionType.SCROLL_BACKWARD))
        node = node.parent;

      if (node)
        node.scrollBackward();
      break;
    case 'scrollForward':
      var node = current.start.node;
      while (node &&
             !node.standardActions.includes(
                 chrome.automation.ActionType.SCROLL_FORWARD))
        node = node.parent;

      if (node)
        node.scrollForward();
      break;

    // These commands are only available when invoked from touch.
    case 'nextAtGranularity':
    case 'previousAtGranularity':
      var backwards = command == 'previousAtGranularity';
      switch (GestureCommandHandler.granularity) {
        case GestureGranularity.CHARACTER:
          command = backwards ? 'previousCharacter' : 'nextCharacter';
          break;
        case GestureGranularity.WORD:
          command = backwards ? 'previousWord' : 'nextWord';
          break;
        case GestureGranularity.LINE:
          command = backwards ? 'previousLine' : 'nextLine';
          break;
      }
      CommandHandler.onCommand(command);
      return false;
    case 'nextGranularity':
    case 'previousGranularity':
      var backwards = command == 'previousGranularity';
      var gran = GestureCommandHandler.granularity;
      var next = backwards ?
          (--gran >= 0 ? gran : GestureGranularity.COUNT - 1) :
          ++gran % GestureGranularity.COUNT;
      GestureCommandHandler.granularity =
          /** @type {GestureGranularity} */ (next);

      var announce = '';
      switch (GestureCommandHandler.granularity) {
        case GestureGranularity.CHARACTER:
          announce = Msgs.getMsg('character_granularity');
          break;
        case GestureGranularity.WORD:
          announce = Msgs.getMsg('word_granularity');
          break;
        case GestureGranularity.LINE:
          announce = Msgs.getMsg('line_granularity');
          break;
      }
      cvox.ChromeVox.tts.speak(announce, cvox.QueueMode.FLUSH);
      return false;
    default:
      return true;
  }

  if (didNavigate)
    chrome.metricsPrivate.recordUserAction('Accessibility.ChromeVox.Navigate');

  if (pred) {
    chrome.metricsPrivate.recordUserAction('Accessibility.ChromeVox.Jump');

    var bound = current.getBound(dir).node;
    if (bound) {
      var node = null;

      if (matchCurrent && pred(bound))
        node = bound;

      if (!node) {
        node = AutomationUtil.findNextNode(
            bound, dir, pred, {skipInitialAncestry: true, root: rootPred});
      }

      if (node && !skipSync) {
        node = AutomationUtil.findNodePre(
                   node, Dir.FORWARD, AutomationPredicate.object) ||
            node;
      }

      if (node) {
        current = cursors.Range.fromNode(node);
      } else {
        cvox.ChromeVox.earcons.playEarcon(cvox.Earcon.WRAP);
        if (!shouldWrap) {
          if (predErrorMsg) {
            new Output()
                .withString(Msgs.getMsg(predErrorMsg))
                .withQueueMode(cvox.QueueMode.FLUSH)
                .go();
          }
          return false;
        }

        var root = bound;
        while (root && !AutomationPredicate.rootOrEditableRoot(root))
          root = root.parent;

        if (!root)
          root = bound.root;

        if (dir == Dir.FORWARD) {
          bound = root;
        } else {
          bound = AutomationUtil.findNodePost(
                      root, dir, AutomationPredicate.leaf) ||
              bound;
        }
        node = AutomationUtil.findNextNode(
            bound, dir, pred, {skipInitialAncestry: true, root: rootPred});

        if (node && !skipSync) {
          node = AutomationUtil.findNodePre(
                     node, Dir.FORWARD, AutomationPredicate.object) ||
              node;
        }

        if (node) {
          current = cursors.Range.fromNode(node);
        } else if (predErrorMsg) {
          new Output()
              .withString(Msgs.getMsg(predErrorMsg))
              .withQueueMode(cvox.QueueMode.FLUSH)
              .go();
          return false;
        }
      }
    }
  }

  if (tryScrolling && current && current.start && current.start.node &&
      ChromeVoxState.instance.currentRange.start.node) {
    var exited = AutomationUtil.getUniqueAncestors(
        current.start.node, ChromeVoxState.instance.currentRange.start.node);
    var scrollable = null;
    for (var i = 0; i < exited.length; i++) {
      if (AutomationPredicate.autoScrollable(exited[i])) {
        scrollable = exited[i];
        break;
      }
    }

    // TODO(dtseng): handle more precise positioning after scroll e.g. list with
    // 10 items shoing 1-7, scroll forward, should position at item 8.
    if (scrollable) {
      var callback = function(result) {
        if (result) {
          var innerCallback = function(currentNode, evt) {
            scrollable.removeEventListener(
                EventType.SCROLL_POSITION_CHANGED, innerCallback);

            if (pred || (currentNode && currentNode.root)) {
              // Jump or if there is a valid current range, then move from it
              // since we have refreshed node data.
              CommandHandler.onCommand(command);
              return;
            }

            // Otherwise, sync to the directed deepest child.
            var sync = scrollable;
            if (dir == Dir.FORWARD) {
              while (sync.firstChild)
                sync = sync.firstChild;
            } else {
              while (sync.lastChild)
                sync = sync.lastChild;
            }
            ChromeVoxState.instance.navigateToRange(
                cursors.Range.fromNode(sync), false, speechProps);
          }.bind(this, current.start.node);
          scrollable.addEventListener(
              EventType.SCROLL_POSITION_CHANGED, innerCallback, true);
        } else {
          ChromeVoxState.instance.navigateToRange(current, false, speechProps);
        }
      };

      if (dir == Dir.FORWARD)
        scrollable.scrollForward(callback);
      else
        scrollable.scrollBackward(callback);
      return false;
    }
  }

  if (current)
    ChromeVoxState.instance.navigateToRange(current, undefined, speechProps);

  return false;
};

/**
 * Increase or decrease a speech property and make an announcement.
 * @param {string} propertyName The name of the property to change.
 * @param {boolean} increase If true, increases the property value by one
 *     step size, otherwise decreases.
 * @private
 */
CommandHandler.increaseOrDecreaseSpeechProperty_ = function(
    propertyName, increase) {
  cvox.ChromeVox.tts.increaseOrDecreaseProperty(propertyName, increase);
};

/**
 * To support viewGraphicAsBraille_(), the current image node.
 * @type {AutomationNode?};
 */
CommandHandler.imageNode_;

/**
 * Called when an image frame is received on a node.
 * @param {!(AutomationEvent|CustomAutomationEvent)} event The event.
 * @private
 */
CommandHandler.onImageFrameUpdated_ = function(event) {
  var target = event.target;
  if (target != CommandHandler.imageNode_)
    return;

  if (!AutomationUtil.isDescendantOf(
          ChromeVoxState.instance.currentRange.start.node,
          CommandHandler.imageNode_)) {
    CommandHandler.imageNode_.removeEventListener(
        EventType.IMAGE_FRAME_UPDATED, CommandHandler.onImageFrameUpdated_,
        false);
    CommandHandler.imageNode_ = null;
    return;
  }

  if (target.imageDataUrl) {
    cvox.ChromeVox.braille.writeRawImage(target.imageDataUrl);
    cvox.ChromeVox.braille.freeze();
  }
};

/**
 * Handle the command to view the first graphic within the current range
 * as braille.
 * @param {!cursors.Range} current The current range.
 * @private
 */
CommandHandler.viewGraphicAsBraille_ = function(current) {
  if (CommandHandler.imageNode_) {
    CommandHandler.imageNode_.removeEventListener(
        EventType.IMAGE_FRAME_UPDATED, CommandHandler.onImageFrameUpdated_,
        false);
    CommandHandler.imageNode_ = null;
  }

  // Find the first node within the current range that supports image data.
  var imageNode = AutomationUtil.findNodePost(
      current.start.node, Dir.FORWARD, AutomationPredicate.supportsImageData);
  if (!imageNode)
    return;

  imageNode.addEventListener(
      EventType.IMAGE_FRAME_UPDATED, CommandHandler.onImageFrameUpdated_,
      false);
  CommandHandler.imageNode_ = imageNode;
  if (imageNode.imageDataUrl) {
    var event = new CustomAutomationEvent(
        EventType.IMAGE_FRAME_UPDATED, imageNode, 'page');
    CommandHandler.onImageFrameUpdated_(event);
  } else {
    imageNode.getImageData(0, 0);
  }
};

/**
 * Provides a partial mapping from ChromeVox key combinations to
 * Search-as-a-function key as seen in Chrome OS documentation.
 * @param {string} command
 * @return {boolean} True if the command should propagate.
 * @private
 */
CommandHandler.onEditCommand_ = function(command) {
  var current = ChromeVoxState.instance.currentRange;
  if (cvox.ChromeVox.isStickyModeOn() || !current || !current.start ||
      !current.start.node || !current.start.node.state[StateType.EDITABLE])
    return true;

  var textEditHandler = DesktopAutomationHandler.instance.textEditHandler;
  if (!textEditHandler)
    return true;

  // Skip customized keys for read only text fields.
  if (textEditHandler.node.restriction ==
      chrome.automation.Restriction.READ_ONLY)
    return true;

  // Skips customized keys if they get suppressed in speech.
  if (AutomationPredicate.shouldOnlyOutputSelectionChangeInBraille(
          textEditHandler.node))
    return true;

  var isMultiline = AutomationPredicate.multiline(current.start.node);
  switch (command) {
    case 'previousCharacter':
      BackgroundKeyboardHandler.sendKeyPress(36, {shift: true});
      break;
    case 'nextCharacter':
      BackgroundKeyboardHandler.sendKeyPress(35, {shift: true});
      break;
    case 'previousWord':
      BackgroundKeyboardHandler.sendKeyPress(36, {shift: true, ctrl: true});
      break;
    case 'nextWord':
      BackgroundKeyboardHandler.sendKeyPress(35, {shift: true, ctrl: true});
      break;
    case 'previousObject':
      if (!isMultiline || textEditHandler.isSelectionOnFirstLine())
        return true;
      BackgroundKeyboardHandler.sendKeyPress(36);
      break;
    case 'nextObject':
      if (!isMultiline)
        return true;

      if (textEditHandler.isSelectionOnLastLine()) {
        textEditHandler.moveToAfterEditText();
        return false;
      }

      BackgroundKeyboardHandler.sendKeyPress(35);
      break;
    case 'previousLine':
      if (!isMultiline || textEditHandler.isSelectionOnFirstLine())
        return true;
      BackgroundKeyboardHandler.sendKeyPress(33);
      break;
    case 'nextLine':
      if (!isMultiline)
        return true;

      if (textEditHandler.isSelectionOnLastLine()) {
        textEditHandler.moveToAfterEditText();
        return false;
      }

      BackgroundKeyboardHandler.sendKeyPress(34);
      break;
    case 'jumpToTop':
      BackgroundKeyboardHandler.sendKeyPress(36, {ctrl: true});
      break;
    case 'jumpToBottom':
      BackgroundKeyboardHandler.sendKeyPress(35, {ctrl: true});
      break;
    default:
      return true;
  }
  return false;
};

/**
 * Performs global initialization.
 */
CommandHandler.init = function() {
  cvox.ChromeVoxKbHandler.commandHandler = CommandHandler.onCommand;
  var firstRunId = 'jdgcneonijmofocbhmijhacgchbihela';
  chrome.runtime.onMessageExternal.addListener(function(
      request, sender, sendResponse) {
    if (sender.id != firstRunId)
      return;

    if (request.openTutorial) {
      var launchTutorial = function(desktop, evt) {
        desktop.removeEventListener(
            chrome.automation.EventType.FOCUS, launchTutorial, true);
        CommandHandler.onCommand('help');
      };

      // Since we get this command early on ChromeVox launch, the first run
      // UI is not yet shown. Monitor for when first run gets focused, and
      // show our tutorial.
      chrome.automation.getDesktop(function(desktop) {
        launchTutorial = launchTutorial.bind(this, desktop);
        desktop.addEventListener(
            chrome.automation.EventType.FOCUS, launchTutorial, true);
      });
    }
  });
};

});  //  goog.scope
