// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The ChromeVox panel and menus.
 */
import {BridgeHelper} from '/common/bridge_helper.js';
import {BrowserUtil} from '/common/browser_util.js';
import {constants} from '/common/constants.js';
import {LocalStorage} from '/common/local_storage.js';

import {BackgroundBridge} from '../common/background_bridge.js';
import {BridgeConstants} from '../common/bridge_constants.js';
import {Command} from '../common/command.js';
import {Msgs} from '../common/msgs.js';
import type {PanelCommand} from '../common/panel_command.js';
import {PanelCommandType} from '../common/panel_command.js';
import type {MenuDataForTest, PanelNodeMenuItemData} from '../common/panel_menu_data.js';
import {SettingsManager} from '../common/settings_manager.js';

import {ISearchUI} from './i_search_ui.js';
import {MenuManager} from './menu_manager.js';
import {MenuManagerWithTestEndpoints} from './menu_manager_with_test_endpoints.js';
import {PanelCaptions} from './panel_captions.js';
import {PanelInterface} from './panel_interface.js';
import {PanelMode, PanelModeInfo} from './panel_mode.js';

const $ = (id: string): HTMLElement | null => document.getElementById(id);

type AsyncCallback = () => Promise<void>;
type SessionState = chrome.loginState.SessionState;

/** Class to manage the panel. */
export class Panel implements PanelInterface {
  private menuManager_ = new MenuManager();
  private mode_: PanelMode = PanelMode.COLLAPSED;
  private originalStickyState_ = false;
  private pendingCallback_: AsyncCallback | null = null;
  private sessionState_ = '';
  private tutorial_: Object|null = null;
  // TODO(crbug.com/388867840): encapsulate these testing members (and others
  // below) in a separate class.
  private userActionMonitorCreatedCount_ = 0;
  private userActionMonitorDestroyedCount_ = 0;
  private isForcedActionPathActive_ = false;

  private brailleContainer_ = $('braille-container');
  private brailleTableElement_ = $('braille-table') as HTMLTableElement;
  private brailleTableElement2_ = $('braille-table2') as HTMLTableElement;
  private searchContainer_ = $('search-container');
  private searchInput_ = $('search') as HTMLInputElement;
  private speechContainer_ = $('speech-container');
  private speechElement_ = $('speech');
  private tutorialReadyForTesting_ = false;

  getMenuManagerForTesting(): MenuManager {
    return this.menuManager_;
  }

  private constructor() {
    this.initListeners_();
  }

  private initListeners_(): void {
    chrome.loginState.getSessionState(
        (state: SessionState) => this.updateSessionState_(state));
    chrome.loginState.onSessionStateChanged.addListener(
        (state: SessionState) => this.updateSessionState_(state));
    // TODO(b/314203187): Not null asserted, check that this is correct.
    $('braille-pan-left')!
        .addEventListener('click', () => this.onPanLeft_(), false);
    $('braille-pan-right')!
        .addEventListener('click', () => this.onPanRight_(), false);
    $('menus_button')!
        .addEventListener(
            'mousedown',
            (event: MouseEvent) => this.menuManager_.onOpenMenus(event), false);
    $('options')!.addEventListener('click', () => this.onOptions_(), false);
    $('close')!.addEventListener('click', () => this.onClose(), false);

    document.addEventListener(
        'keydown', (event: KeyboardEvent) => this.onKeyDown_(event), false);
    document.addEventListener(
        'mouseup', (event: MouseEvent) => this.menuManager_.onMouseUp(event),
        false);
    window.addEventListener(
        'storage', (event: StorageEvent) => this.onStorageChanged_(event),
        false);

    window.addEventListener('blur', event => this.onBlur_(event), false);
    window.addEventListener('hashchange', () => this.onHashChange_(), false);

    BridgeHelper.registerHandler(
        BridgeConstants.Panel.TARGET,
        BridgeConstants.Panel.Action.IS_PANEL_INITIALIZED, () => {return true});
    BridgeHelper.registerHandler(
        BridgeConstants.Panel.TARGET, BridgeConstants.Panel.Action.EXEC_COMMAND,
        (panelCommand: PanelCommand) => this.exec_(panelCommand))
    BridgeHelper.registerHandler(
        BridgeConstants.Panel.TARGET,
        BridgeConstants.Panel.Action.ADD_MENU_ITEM,
        (itemData: PanelNodeMenuItemData) => this.menuManager_.addNodeMenuItem(
            itemData));
    BridgeHelper.registerHandler(
        BridgeConstants.Panel.TARGET,
        BridgeConstants.Panel.Action.ON_CURRENT_RANGE_CHANGED,
        () => this.onCurrentRangeChanged_());
    this.updateFromPrefs_();

    // These handlers are all for tests.
    BridgeHelper.registerHandler(
        BridgeConstants.PanelTest.TARGET,
        BridgeConstants.PanelTest.Action.BRAILLE_PAN_LEFT,
        () => this.onPanLeft_());
    BridgeHelper.registerHandler(
        BridgeConstants.PanelTest.TARGET,
        BridgeConstants.PanelTest.Action.BRAILLE_PAN_RIGHT,
        () => this.onPanRight_());
    BridgeHelper.registerHandler(
        BridgeConstants.PanelTest.TARGET,
        BridgeConstants.PanelTest.Action.DISABLE_ERROR_MSG,
        () => this.disableErrorMsgForTest_());
    BridgeHelper.registerHandler(
        BridgeConstants.PanelTest.TARGET,
        BridgeConstants.PanelTest.Action.DISABLE_TUTORIAL_RESTART_NUDGES,
        () => this.disableTutorialRestartNudgesForTest_());
    BridgeHelper.registerHandler(
        BridgeConstants.PanelTest.TARGET,
        BridgeConstants.PanelTest.Action.FIRE_MOCK_EVENT,
        (key: string) => this.fireMockEventForTest_(key));
    BridgeHelper.registerHandler(
        BridgeConstants.PanelTest.TARGET,
        BridgeConstants.PanelTest.Action.FIRE_MOCK_QUERY,
        (query: string) => this.fireMockQueryForTest_(query));
    BridgeHelper.registerHandler(
        BridgeConstants.PanelTest.TARGET,
        BridgeConstants.PanelTest.Action.GET_ACTIVE_MENU_DATA,
        () => this.getActiveMenuDataForTest_());
    BridgeHelper.registerHandler(
        BridgeConstants.PanelTest.TARGET,
        BridgeConstants.PanelTest.Action.GET_ACTIVE_SEARCH_MENU_DATA,
        () => this.getActiveSearchMenuDataForTest_());
    BridgeHelper.registerHandler(
        BridgeConstants.PanelTest.TARGET,
        BridgeConstants.PanelTest.Action.GET_TUTORIAL_ACTIVE_LESSON_INDEX,
        () => this.getTutorialActiveLessonIndexForTest_());
    BridgeHelper.registerHandler(
        BridgeConstants.PanelTest.TARGET,
        BridgeConstants.PanelTest.Action.GET_TUTORIAL_ACTIVE_SCREEN,
        () => this.getTutorialActiveScreenForTest_());
    BridgeHelper.registerHandler(
        BridgeConstants.PanelTest.TARGET,
        BridgeConstants.PanelTest.Action.GET_TUTORIAL_INTERACTIVE_MODE,
        () => this.getTutorialInteractiveModeForTest_());
    BridgeHelper.registerHandler(
        BridgeConstants.PanelTest.TARGET,
        BridgeConstants.PanelTest.Action.GET_TUTORIAL_READY,
        () => this.tutorialReadyForTesting_);
    BridgeHelper.registerHandler(
        BridgeConstants.PanelTest.TARGET,
        BridgeConstants.PanelTest.Action.GET_FORCED_ACTION_PATH_CREATED_COUNT,
        () => this.getForcedActionPathCreatedCountForTest_());
    BridgeHelper.registerHandler(
        BridgeConstants.PanelTest.TARGET,
        BridgeConstants.PanelTest.Action.GET_FORCED_ACTION_PATH_DESTROYED_COUNT,
        () => this.getForcedActionPathDestroyedCountForTest_());
    BridgeHelper.registerHandler(
        BridgeConstants.PanelTest.TARGET,
        BridgeConstants.PanelTest.Action.GET_IS_FORCED_ACTION_PATH_ACTIVE,
        () => this.getIsForcedActionPathActiveForTest_());
    BridgeHelper.registerHandler(
        BridgeConstants.PanelTest.TARGET,
        BridgeConstants.PanelTest.Action.GIVE_TUTORIAL_NUDGE,
        () => this.giveTutorialNudgeForTest_());
    BridgeHelper.registerHandler(
        BridgeConstants.PanelTest.TARGET,
        BridgeConstants.PanelTest.Action.INITIALIZE_TUTORIAL_NUDGES,
        (context: string) => this.initializeTutorialNudgesForTest_(context));
    BridgeHelper.registerHandler(
        BridgeConstants.PanelTest.TARGET,
        BridgeConstants.PanelTest.Action.REPLACE_MENU_MANAGER,
        () => this.replaceMenuManager_());
    BridgeHelper.registerHandler(
        BridgeConstants.PanelTest.TARGET,
        BridgeConstants.PanelTest.Action.RESTART_TUTORIAL_NUDGES,
        () => this.restartTutorialNudgesForTest_());
    BridgeHelper.registerHandler(
        BridgeConstants.PanelTest.TARGET,
        BridgeConstants.PanelTest.Action.SWAP_FORCED_ACTION_PATH_METHODS,
        () => this.swapForcedActionPathMethodsForTest_());
    BridgeHelper.registerHandler(
        BridgeConstants.PanelTest.TARGET,
        BridgeConstants.PanelTest.Action.SET_TUTORIAL_CURRICULUM,
        (curriculum: string) => this.setTutorialCurriculumForTest_(curriculum));
    BridgeHelper.registerHandler(
        BridgeConstants.PanelTest.TARGET,
        BridgeConstants.PanelTest.Action.SET_TUTORIAL_MEDIUM,
        (medium: string) => this.setTutorialMediumForTest_(medium));
    BridgeHelper.registerHandler(
        BridgeConstants.PanelTest.TARGET,
        BridgeConstants.PanelTest.Action.SHOW_TUTORIAL_LESSON,
        (lessonNum: number) => this.showTutorialLessonForTest_(lessonNum));
    BridgeHelper.registerHandler(
        BridgeConstants.PanelTest.TARGET,
        BridgeConstants.PanelTest.Action.SHOW_TUTORIAL_LESSON_MENU,
        () => this.showTutorialLessonMenuForTest_());
    BridgeHelper.registerHandler(
        BridgeConstants.PanelTest.TARGET,
        BridgeConstants.PanelTest.Action.SHOW_TUTORIAL_MAIN_MENU,
        () => this.showTutorialMainMenuForTest_());
    BridgeHelper.registerHandler(
        BridgeConstants.PanelTest.TARGET,
        BridgeConstants.PanelTest.Action.SHOW_TUTORIAL_NEXT_LESSON,
        () => this.showTutorialNextLessonForTest_());
  }

  /** Initialize the panel. */
  static async init(): Promise<void> {
    if (Panel.instance) {
      throw new Error('Cannot call Panel.init() more than once');
    }

    await LocalStorage.init();
    await SettingsManager.init();

    PanelCaptions.init();

    Panel.instance = new Panel();
    PanelInterface.instance = Panel.instance;

    Msgs.addTranslatedMessagesToDom(document);

    if (location.search.slice(1) === 'tutorial') {
      Panel.instance.onTutorial_();
    }
  }

  setPendingCallback(callback: AsyncCallback | null): void {
    this.pendingCallback_ = callback;
  }

  get mode(): PanelMode {
    return this.mode_;
  }

  get sessionState(): string {
    return this.sessionState_;
  }

  /**
   * Update the display based on prefs.
   * TODO(b/314203187): Not nulls asserted, check that this is correct.
   */
  private updateFromPrefs_(): void {
    if (this.mode_ === PanelMode.SEARCH) {
      this.speechContainer_!.hidden = true;
      this.brailleContainer_!.hidden = true;
      this.searchContainer_!.hidden = false;
      return;
    }

    this.speechContainer_!.hidden = false;
    this.brailleContainer_!.hidden = false;
    this.searchContainer_!.hidden = true;

    if (LocalStorage.get('brailleCaptions')) {
      this.speechContainer_!.style.visibility = 'hidden';
      this.brailleContainer_!.style.visibility = 'visible';
    } else {
      this.speechContainer_!.style.visibility = 'visible';
      this.brailleContainer_!.style.visibility = 'hidden';
    }
  }

  private disableErrorMsgForTest_() {
    MenuManager.disableMissingMsgsErrorsForTesting = true;
  }

  private disableTutorialRestartNudgesForTest_(): void {
    if (this.tutorial_) {
      (this.tutorial_ as {restartNudges: (() => void) | null}).restartNudges =
          null;
    }
  }

  private fireMockEventForTest_(key: string): void {
    // @ts-ignore: Mocked KeyboardEvent.
    const obj: KeyboardEvent = {key};
    obj.preventDefault = function() {};
    obj.stopPropagation = function() {};
    this.onKeyDown_(obj);
  }

  private fireMockQueryForTest_(query: string): void {
    // @ts-ignore: Mocked InputEvent.
    const evt: InputEvent = {target: {value: query}};
    this.menuManager_.onSearchBarQuery(evt);
  }

  private getActiveMenuDataForTest_(): MenuDataForTest {
    const activeMenu = this.menuManager_.activeMenu;
    if (activeMenu) {
      return activeMenu.getMenuDataForTest();
    }
    return {};
  }

  private getActiveSearchMenuDataForTest_(): MenuDataForTest {
    const searchMenu = this.menuManager_.searchMenu;
    if (searchMenu) {
      return searchMenu.getMenuDataForTest();
    }
    return {};
  }

  private getTutorialActiveLessonIndexForTest_(): number {
    if (this.tutorial_) {
      return (this.tutorial_ as {activeLessonIndex: number}).activeLessonIndex;
    }
    return -1;
  }

  private getTutorialActiveScreenForTest_(): string {
    if (this.tutorial_) {
      return (this.tutorial_ as {activeScreen: string}).activeScreen;
    }
    return '';
  }

  private getTutorialInteractiveModeForTest_(): boolean {
    if (this.tutorial_) {
      return (this.tutorial_ as {interactiveMode_: boolean}).interactiveMode_;
    }
    return false;
  }

  private getForcedActionPathCreatedCountForTest_(): number {
    return this.userActionMonitorCreatedCount_;
  }

  private getForcedActionPathDestroyedCountForTest_(): number {
    return this.userActionMonitorDestroyedCount_;
  }

  private getIsForcedActionPathActiveForTest_(): boolean {
    return this.isForcedActionPathActive_;
  }

  private giveTutorialNudgeForTest_(): void {
    if (this.tutorial_) {
      (this.tutorial_ as {giveNudge: () => void}).giveNudge();
    }
  }

  private initializeTutorialNudgesForTest_(context: string): void {
    if (this.tutorial_) {
      (this.tutorial_ as {
        initializeNudges: (context: string) => void
      }).initializeNudges(context);
    }
  }

  private replaceMenuManager_() {
    this.menuManager_ = new MenuManagerWithTestEndpoints();
  }

  private restartTutorialNudgesForTest_(): Promise<void> {
    return new Promise(resolve => {
      if (this.tutorial_) {
        (this.tutorial_ as {restartNudges: () => void}).restartNudges = resolve;
      }
    });
  }

  private swapForcedActionPathMethodsForTest_(): void {
    this.userActionMonitorCreatedCount_ = 0;
    this.userActionMonitorDestroyedCount_ = 0;
    this.isForcedActionPathActive_ = false;
    BackgroundBridge.ForcedActionPath.listenFor = () => {
      this.userActionMonitorCreatedCount_ += 1;
      this.isForcedActionPathActive_ = true;
      return new Promise((resolve) => {
        // In production, this will resolve when the forced action path has
        // been fulfilled. For example, if the forced action path is for the
        // user to press the space bar, then this will resolve when the space
        // bar is pressed. For the purposes of testing, we don't want this to
        // resolve because that gives a false signal that the forced action
        // path has been completed. To prevent this from resolving, we use a
        // timeout.
        setTimeout(resolve, 30 * 1000);
      })
    };
    BackgroundBridge.ForcedActionPath.stopListening = () => {
      this.userActionMonitorDestroyedCount_ += 1;
      this.isForcedActionPathActive_ = false;
      return Promise.resolve();
    };
  }

  private setTutorialCurriculumForTest_(curriculum: string): void {
    if (this.tutorial_) {
      (this.tutorial_ as {curriculum: string}).curriculum = curriculum;
    }
  }

  private setTutorialMediumForTest_(medium: string): void {
    if (this.tutorial_) {
      (this.tutorial_ as {medium: string}).medium = medium;
    }
  }

  private showTutorialLessonForTest_(lessonNum: number): void {
    if (this.tutorial_) {
      (this.tutorial_ as {
        showLesson_: (lesson: number) => void
      }).showLesson_(lessonNum);
    }
  }

  private showTutorialLessonMenuForTest_(): void {
    if (this.tutorial_) {
      (this.tutorial_ as {showLessonMenu_: () => void}).showLessonMenu_();
    }
  }

  private showTutorialMainMenuForTest_(): void {
    if (this.tutorial_) {
      (this.tutorial_ as {showMainMenu_: () => void}).showMainMenu_();
    }
  }

  private showTutorialNextLessonForTest_(): void {
    if (this.tutorial_) {
      (this.tutorial_ as {showNextLesson: () => void}).showNextLesson();
    }
  }


  /**
   * Execute a command to update the panel.
   * TODO(b/314203187): Not nulls asserted, check that this is correct.
   */
  private exec_(command: PanelCommand): void {
    /**
     * Escape text so it can be safely added to HTML.
     * @param str Text to be added to HTML, will be cast to string.
     * @return The escaped string.
     */
    function escapeForHtml(str: string): string {
      return String(str)
          .replace(/&/g, '&amp;')
          .replace(/</g, '&lt;')
          .replace(/\>/g, '&gt;')
          .replace(/"/g, '&quot;')
          .replace(/'/g, '&#039;')
          .replace(/\//g, '&#x2F;');
    }

    switch (command.type) {
      case PanelCommandType.CLEAR_SPEECH:
        this.speechElement_!.innerHTML = '';
        break;
      case PanelCommandType.ADD_NORMAL_SPEECH:
        if (this.speechElement_!.innerHTML !== '') {
          this.speechElement_!.innerHTML += '&nbsp;&nbsp;';
        }
        this.speechElement_!.innerHTML +=
            '<span class="usertext">' +
            escapeForHtml(command.data as string) + '</span>';
        break;
      case PanelCommandType.ADD_ANNOTATION_SPEECH:
        if (this.speechElement_!.innerHTML !== '') {
          this.speechElement_!.innerHTML += '&nbsp;&nbsp;';
        }
        this.speechElement_!.innerHTML += escapeForHtml(command.data as string);
        break;
      case PanelCommandType.UPDATE_BRAILLE:
        this.onUpdateBraille_(
            command.data as {groups: string[][], cols: number, rows: number});
        break;
      case PanelCommandType.OPEN_MENUS:
        this.menuManager_.onOpenMenus(undefined, String(command.data));
        break;
      case PanelCommandType.OPEN_MENUS_MOST_RECENT:
        this.menuManager_.onOpenMenus(undefined, this.menuManager_.lastMenu);
        break;
      case PanelCommandType.SEARCH:
        this.onSearch_();
        break;
      case PanelCommandType.TUTORIAL:
        this.onTutorial_();
        break;
      case PanelCommandType.CLOSE_CHROMEVOX:
        this.onClose();
        break;
    }
  }

  /**
   * Sets the mode, which determines the size of the panel and what objects
   *     are shown or hidden.
   * TODO(b/314203187): Not nulls asserted, check that this is correct.
   */
  setMode(mode: PanelMode): void {
    if (this.mode_ === mode) {
      return;
    }

    // Change the title of ChromeVox menu based on menu's state.
    $('menus_title')!
        .setAttribute(
            'msgid',
            mode === PanelMode.FULLSCREEN_MENUS ? 'menus_collapse_title' :
                                                  'menus_title');
    Msgs.addTranslatedMessagesToDom(document);

    this.mode_ = mode;

    document.title = Msgs.getMsg(PanelModeInfo[this.mode_].title);

    // Fully qualify the path here because this function might be called with a
    // window object belonging to the background page.
    window.location.assign(
        chrome.runtime.getURL('chromevox/mv3/panel/panel.html') +
        PanelModeInfo[this.mode_].location);

    $('main')!.hidden = (this.mode_ === PanelMode.FULLSCREEN_TUTORIAL);
    $('menus_background')!.hidden = (this.mode_ !== PanelMode.FULLSCREEN_MENUS);
    // Interactive tutorial elements may not have been loaded yet.
    const iTutorialContainer = $('chromevox-tutorial-container');
    if (iTutorialContainer) {
      iTutorialContainer.hidden =
          (this.mode_ !== PanelMode.FULLSCREEN_TUTORIAL);
    }

    this.updateFromPrefs_();

    // Change the orientation of the triangle next to the menus button to
    // indicate whether the menu is open or closed.
    if (mode === PanelMode.FULLSCREEN_MENUS) {
      $('triangle')!.style.transform = 'rotate(180deg)';
    } else if (mode === PanelMode.COLLAPSED) {
      $('triangle')!.style.transform = '';
    }
  }

  /** Open incremental search. */
  private async onSearch_(): Promise<void> {
    this.setMode(PanelMode.SEARCH);
    this.menuManager_.clearMenus();
    this.pendingCallback_ = null;
    this.updateFromPrefs_();
    await ISearchUI.init(this.searchInput_);
  }

  /**
   * Updates the content shown on the virtual braille display.
   * @param data The data sent through the PanelCommand.
   * TODO(b/314203187): Not nulls asserted, check that this is correct.
   */
  private onUpdateBraille_(
      data: {groups: string[][], cols: number, rows: number}): void {
    const {groups, cols, rows} = data;
    const sideBySide = SettingsManager.get('brailleSideBySide');

    this.brailleContainer_!.addEventListener(
        'mouseover',
        event => PanelCaptions.braille.addBorders(
            event.target as HTMLTableCellElement));
    this.brailleContainer_!.addEventListener(
        'mouseout',
        event => PanelCaptions.braille.removeBorders(
            event.target as HTMLTableCellElement));
    this.brailleContainer_!.addEventListener(
        'click',
        event => PanelCaptions.braille.routeCursor(
            event.target as HTMLTableCellElement));

    PanelCaptions.braille.clearTables();

    let row1;
    let row2;
    // Number of rows already written.
    let rowCount = 0;
    // Number of cells already written in this row.
    let cellCount = cols;
    for (let i = 0; i < groups.length; i++) {
      if (cellCount === cols) {
        cellCount = 0;
        // Check if we reached the limit on the number of rows we can have.
        if (rowCount === rows) {
          break;
        }
        rowCount++;
        row1 = this.brailleTableElement_.insertRow(-1);
        if (sideBySide) {
          // Side by side.
          row2 = this.brailleTableElement2_.insertRow(-1);
        } else {
          // Interleaved.
          row2 = this.brailleTableElement_.insertRow(-1);
        }
      }

      const topCell = row1!.insertCell(-1);
      topCell.innerHTML = groups[i][0];
      topCell.id = i + '-textCell';
      topCell.setAttribute('data-companionIDs', i + '-brailleCell');
      topCell.className = 'unhighlighted-cell';

      let bottomCell = row2!.insertCell(-1);
      bottomCell.id = i + '-brailleCell';
      bottomCell.setAttribute('data-companionIDs', i + '-textCell');
      bottomCell.className = 'unhighlighted-cell';
      if (cellCount + groups[i][1].length > cols) {
        let brailleText = groups[i][1];
        while (cellCount + brailleText.length > cols) {
          // At this point we already have a bottomCell to fill, so fill it.
          bottomCell.innerHTML = brailleText.substring(0, cols - cellCount);
          // Update to see what we still have to fill.
          brailleText = brailleText.substring(cols - cellCount);
          // Make new row.
          if (rowCount === rows) {
            break;
          }
          rowCount++;
          row1 = this.brailleTableElement_.insertRow(-1);
          if (sideBySide) {
            // Side by side.
            row2 = this.brailleTableElement2_.insertRow(-1);
          } else {
            // Interleaved.
            row2 = this.brailleTableElement_.insertRow(-1);
          }
          const bottomCell2 = row2.insertCell(-1);
          bottomCell2.id = i + '-brailleCell2';
          bottomCell2.setAttribute(
              'data-companionIDs', i + '-textCell ' + i + '-brailleCell');
          bottomCell.setAttribute(
              'data-companionIDs',
              bottomCell.getAttribute('data-companionIDs') + ' ' + i +
                  '-brailleCell2');
          topCell.setAttribute(
              'data-companionID2',
              bottomCell.getAttribute('data-companionIDs') + ' ' + i +
                  '-brailleCell2');

          bottomCell2.className = 'unhighlighted-cell';
          bottomCell = bottomCell2;
          cellCount = 0;
        }
        // Fill the rest.
        bottomCell.innerHTML = brailleText;
        cellCount = brailleText.length;
      } else {
        bottomCell.innerHTML = groups[i][1];
        cellCount += groups[i][1].length;
      }
    }
  }

  /**
   * Called when a key is pressed. Handle arrow keys to navigate the menus,
   * Esc to close, and Enter/Space to activate an item.
   */
  private onKeyDown_(event: KeyboardEvent): void {
    if (event.key === 'Escape' &&
        this.mode_ === PanelMode.FULLSCREEN_TUTORIAL) {
      this.setMode(PanelMode.COLLAPSED);
      return;
    }

    if (!this.menuManager_.onKeyDown(event)) {
      return;
    }

    event.preventDefault();
    event.stopPropagation();
  }

  /**
   * Open the ChromeVox Options.
   * TODO: Remove this once settings migration is complete.
   */
  private onOptions_(): void {
    chrome.accessibilityPrivate.openSettingsSubpage('textToSpeech/chromeVox');
    this.setMode(PanelMode.COLLAPSED);
  }

  onClose(): void {
    // Change the url fragment to 'close', which signals the native code
    // to exit ChromeVox.
    window.location.assign(
        chrome.runtime.getURL('chromevox/mv3/panel/panel.html') + '#close');
  }

  async closeMenusAndRestoreFocus(): Promise<void> {
    const pendingCallback = this.pendingCallback_;
    this.pendingCallback_ = null;

    // Prepare the watcher before close the panel so that the watcher won't miss
    // panel collapse signal.
    await BackgroundBridge.PanelBackground.setPanelCollapseWatcher();

    // Make sure all menus are cleared to avoid bogus output when we re-open.
    this.menuManager_.clearMenus();

    // Make sure we're not in full-screen mode.
    this.setMode(PanelMode.COLLAPSED);

    await BackgroundBridge.PanelBackground.waitForPanelCollapse();

    if (pendingCallback) {
      await pendingCallback();
    }
    BackgroundBridge.PanelBackground.clearSavedNode();
  }

  /** Open the tutorial. */
  private onTutorial_(): void {
    chrome.chromeosInfoPrivate.isTabletModeEnabled((enabled: boolean) => {
      // Use tablet mode to decide the medium for the tutorial.
      const medium = enabled ? constants.InteractionMedium.TOUCH :
                               constants.InteractionMedium.KEYBOARD;
      if (!$('chromevox-tutorial')) {
        let curriculum: string | null = null;
        if (this.sessionState_ ===
            chrome.loginState.SessionState.IN_OOBE_SCREEN) {
          // We currently support two mediums: keyboard and touch, which is why
          // we can decide the curriculum using a ternary statement.
          curriculum = medium === constants.InteractionMedium.KEYBOARD ?
              'quick_orientation' :
              'touch_orientation';
        }
        this.createITutorial_(curriculum, medium);
      }

      this.setMode(PanelMode.FULLSCREEN_TUTORIAL);
      const tutorial = this.tutorial_ as {
        show: VoidFunction,
        medium: constants.InteractionMedium,
      };
      if (tutorial && tutorial.show) {
        tutorial.medium = medium;
        tutorial.show();
      }
    });
  }

  /** Creates a <chromevox-tutorial> element and adds it to the dom. */
  private createITutorial_(
      curriculum: string | null, medium: constants.InteractionMedium): void {
    const tutorialScript = document.createElement('script');
    tutorialScript.src =
        '../../../common/tutorial/components/chromevox_tutorial.js';
    tutorialScript.setAttribute('type', 'module');
    document.body.appendChild(tutorialScript);

    // Create tutorial container and element.
    const tutorialContainer = document.createElement('div');
    tutorialContainer.setAttribute('id', 'chromevox-tutorial-container');
    tutorialContainer.hidden = true;
    const element = document.createElement('chromevox-tutorial');
    element.setAttribute('id', 'chromevox-tutorial');
    const tutorialElement = element as unknown as {
      curriculum: string,
      medium: constants.InteractionMedium,
    };
    if (curriculum) {
      tutorialElement.curriculum = curriculum;
    }
    tutorialElement.medium = medium;
    tutorialContainer.appendChild(element);
    document.body.appendChild(tutorialContainer);
    this.tutorial_ = tutorialElement;

    // Add listeners. These are custom events fired from custom components.

    const elementInPage = $('chromevox-tutorial');
    if (!elementInPage) {
      throw new Error('Tutorial element was not added to the DOM');
    }

    elementInPage.addEventListener('closetutorial', async _evt => {
      // Ensure ForcedActionPath is destroyed before closing tutorial.
      await BackgroundBridge.ForcedActionPath.stopListening();
      this.onCloseTutorial_();
    });
    elementInPage.addEventListener('startinteractivemode', async evt => {
      const actions = (evt as CustomEvent).detail.actions;
      await BackgroundBridge.ForcedActionPath.listenFor(actions);
      await BackgroundBridge.ForcedActionPath.stopListening();
      const tutorial = this.tutorial_ as {showNextLesson: VoidFunction};
      if (tutorial && tutorial.showNextLesson) {
        tutorial.showNextLesson();
      }
    });
    elementInPage.addEventListener('stopinteractivemode', async _evt => {
      await BackgroundBridge.ForcedActionPath.stopListening();
    });
    elementInPage.addEventListener('requestfullydescribe', _evt => {
      BackgroundBridge.CommandHandler.onCommand(Command.FULLY_DESCRIBE);
    });
    elementInPage.addEventListener('requestearcon', evt => {
      const earconId = (evt as CustomEvent).detail.earconId;
      BackgroundBridge.Earcons.playEarcon(earconId);
    });
    elementInPage.addEventListener('cancelearcon', evt => {
      const earconId = (evt as CustomEvent).detail.earconId;
      BackgroundBridge.Earcons.cancelEarcon(earconId);
    });
    elementInPage.addEventListener('readyfortesting', () => {
      this.tutorialReadyForTesting_ = true;
    });
    elementInPage.addEventListener('openUrl', async evt => {
      const url = (evt as CustomEvent).detail.url;
      // Ensure ForcedActionPath is destroyed before closing tutorial.
      await BackgroundBridge.ForcedActionPath.stopListening();
      this.onCloseTutorial_();
      BrowserUtil.openBrowserUrl(url);
    });
  }

  /** Close the tutorial. */
  private onCloseTutorial_(): void {
    this.setMode(PanelMode.COLLAPSED);
  }

  private onCurrentRangeChanged_(): void {
    if (this.mode_ === PanelMode.FULLSCREEN_TUTORIAL) {
      const tutorial = this.tutorial_ as {restartNudges: VoidFunction};
      if (this.tutorial_ && tutorial.restartNudges) {
      tutorial.restartNudges();
      }
    }
  }

  private onBlur_(event: FocusEvent): void {
    const target = event.target as EventTarget | typeof window | null;
    if (target !== window || document.activeElement === document.body) {
      return;
    }

    this.closeMenusAndRestoreFocus();
  }

  private async onHashChange_(): Promise<void> {
    // Save the sticky state when a user first focuses the panel.
    if (location.hash === '#fullscreen' || location.hash === '#focus') {
      this.originalStickyState_ =
          await BackgroundBridge.ChromeVoxPrefs.getStickyPref();
    }

    // If the original sticky state was on when we first entered the panel,
    // toggle it in in every case. (fullscreen/focus turns the state off,
    // collapse turns it back on).
    if (this.originalStickyState_) {
      BackgroundBridge.CommandHandler.onCommand(Command.TOGGLE_STICKY_MODE);
    }
  }

  private async onPanLeft_(): Promise<void> {
    await BackgroundBridge.Braille.panLeft();
  }

  private async onPanRight_(): Promise<void> {
    await BackgroundBridge.Braille.panRight();
  }

  private onStorageChanged_(event: StorageEvent): void {
    if (event.key === 'brailleCaptions') {
      this.updateFromPrefs_();
    }
  }

  private updateSessionState_(sessionState: string): void {
    this.sessionState_ = sessionState;
    const options = $('options') as unknown as {disabled: boolean};
    options.disabled = sessionState !== 'IN_SESSION';
  }
}

export namespace Panel {
  export let instance: Panel | undefined;
}

window.addEventListener('load', async () => await Panel.init(), false);
