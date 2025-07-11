// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides an interface for other renderers to communicate with
 * the ChromeVox panel.
 */
import {BridgeHelper} from '/common/bridge_helper.js';
import {TestImportManager} from '/common/testing/test_import_manager.js';

import {BridgeConstants} from './bridge_constants.js';
import {PanelCommand} from './panel_command.js';
import type {PanelNodeMenuItemData} from './panel_menu_data.js';
import {MenuDataForTest} from './panel_menu_data.js';

const PanelTarget = BridgeConstants.Panel.TARGET;
const PanelAction = BridgeConstants.Panel.Action;

const PanelTestTarget = BridgeConstants.PanelTest.TARGET;
const PanelTestAction = BridgeConstants.PanelTest.Action;

export class PanelBridge {
  static addMenuItem(itemData: PanelNodeMenuItemData): Promise<void> {
    return BridgeHelper.sendMessage(
        PanelTarget, PanelAction.ADD_MENU_ITEM, itemData);
  }

  static async onCurrentRangeChanged(): Promise<void> {
    return BridgeHelper.sendMessage(
        PanelTarget, PanelAction.ON_CURRENT_RANGE_CHANGED);
  }

  static execCommand(panelCommand: PanelCommand): Promise<void> {
    return BridgeHelper.sendMessage(
        PanelTarget, PanelAction.EXEC_COMMAND,
        {type: panelCommand.type, data: panelCommand.data});
  }

  static braillePanLeftForTest(): Promise<void> {
    return BridgeHelper.sendMessage(
        PanelTestTarget, PanelTestAction.BRAILLE_PAN_LEFT);
  }

  static braillePanRightForTest(): Promise<void> {
    return BridgeHelper.sendMessage(
        PanelTestTarget, PanelTestAction.BRAILLE_PAN_RIGHT);
  }

  static disableMessagesForTest(): Promise<void> {
    return BridgeHelper.sendMessage(
        PanelTestTarget, PanelTestAction.DISABLE_ERROR_MSG);
  }

  static disableTutorialRestartNudges(): Promise<void> {
    return BridgeHelper.sendMessage(
        PanelTestTarget, PanelTestAction.DISABLE_TUTORIAL_RESTART_NUDGES);
  }

  static fireMockEventForTest(key: string): Promise<void> {
    return BridgeHelper.sendMessage(
        PanelTestTarget, PanelTestAction.FIRE_MOCK_EVENT, key);
  }

  static fireMockQueryForTest(query: string): Promise<void> {
    return BridgeHelper.sendMessage(
        PanelTestTarget, PanelTestAction.FIRE_MOCK_QUERY, query);
  }

  static getActiveMenuDataForTest(): Promise<MenuDataForTest> {
    return BridgeHelper.sendMessage(
        PanelTestTarget, PanelTestAction.GET_ACTIVE_MENU_DATA);
  }

  static getActiveSearchMenuDataForTest(): Promise<MenuDataForTest> {
    return BridgeHelper.sendMessage(
        PanelTestTarget, PanelTestAction.GET_ACTIVE_SEARCH_MENU_DATA);
  }

  static getTutorialActiveLessonIndex(): Promise<number> {
    return BridgeHelper.sendMessage(
        PanelTestTarget, PanelTestAction.GET_TUTORIAL_ACTIVE_LESSON_INDEX);
  }

  static getTutorialActiveScreen(): Promise<string> {
    return BridgeHelper.sendMessage(
        PanelTestTarget, PanelTestAction.GET_TUTORIAL_ACTIVE_SCREEN);
  }

  static getTutorialInteractiveMode(): Promise<boolean> {
    return BridgeHelper.sendMessage(
        PanelTestTarget, PanelTestAction.GET_TUTORIAL_INTERACTIVE_MODE);
  }

  static getTutorialReadyForTest(): Promise<boolean> {
    return BridgeHelper.sendMessage(
        PanelTestTarget, PanelTestAction.GET_TUTORIAL_READY);
  }

  static getForcedActionPathCreatedCountForTest(): Promise<number> {
    return BridgeHelper.sendMessage(
        PanelTestTarget, PanelTestAction.GET_FORCED_ACTION_PATH_CREATED_COUNT);
  }

  static getForcedActionPathDestroyedCountForTest(): Promise<number> {
    return BridgeHelper.sendMessage(
        PanelTestTarget,
        PanelTestAction.GET_FORCED_ACTION_PATH_DESTROYED_COUNT);
  }

  static getIsForcedActionPathActiveForTest(): Promise<boolean> {
    return BridgeHelper.sendMessage(
        PanelTestTarget, PanelTestAction.GET_IS_FORCED_ACTION_PATH_ACTIVE);
  }

  static giveTutorialNudge(): Promise<void> {
    return BridgeHelper.sendMessage(
        PanelTestTarget, PanelTestAction.GIVE_TUTORIAL_NUDGE);
  }

  static initializeTutorialNudges(context: string): Promise<void> {
    return BridgeHelper.sendMessage(
        PanelTestTarget, PanelTestAction.INITIALIZE_TUTORIAL_NUDGES, context);
  }

  static restartTutorialNudges(): Promise<void> {
    return BridgeHelper.sendMessage(
        PanelTestTarget, PanelTestAction.RESTART_TUTORIAL_NUDGES);
  }

  static setTutorialCurriculum(curriculum: string): Promise<void> {
    return BridgeHelper.sendMessage(
        PanelTestTarget, PanelTestAction.SET_TUTORIAL_CURRICULUM, curriculum);
  }

  static setTutorialMedium(medium: string): Promise<void> {
    return BridgeHelper.sendMessage(
        PanelTestTarget, PanelTestAction.SET_TUTORIAL_MEDIUM, medium);
  }

  static showTutorialLesson(lessonNum: number): Promise<void> {
    return BridgeHelper.sendMessage(
        PanelTestTarget, PanelTestAction.SHOW_TUTORIAL_LESSON, lessonNum);
  }

  static showTutorialLessonMenu(): Promise<void> {
    return BridgeHelper.sendMessage(
        PanelTestTarget, PanelTestAction.SHOW_TUTORIAL_LESSON_MENU);
  }

  static showTutorialMainMenu(): Promise<void> {
    return BridgeHelper.sendMessage(
        PanelTestTarget, PanelTestAction.SHOW_TUTORIAL_MAIN_MENU);
  }

  static showTutorialNextLesson(): Promise<void> {
    return BridgeHelper.sendMessage(
        PanelTestTarget, PanelTestAction.SHOW_TUTORIAL_NEXT_LESSON);
  }

  static swapForcedActionPathMethodsForTesting(): Promise<void> {
    return BridgeHelper.sendMessage(
        PanelTestTarget, PanelTestAction.SWAP_FORCED_ACTION_PATH_METHODS);
  }
}

TestImportManager.exportForTesting(PanelBridge);
