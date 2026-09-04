// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';

import {kBuiltInToolDefinitions} from '../../generated_tool_definitions.js';
import type {WebClientHandlerRemote, WebClientInitialState} from '../../glic.mojom-webui.js';
import {ChromeToolBlockingBehavior, ChromeToolResponseScheduling, ExecuteToolErrorReason, HostCapability} from '../../glic_api/glic_api.js';
import type {ChromeTool, ChromeToolExecutionResult, GlicBrowserHost, GlicToolsHost} from '../../glic_api/glic_api.js';
import {AiOverlayToolsRemote, ScrollGranularity} from '../../tools.mojom-webui.js';
import {hostCapabilitiesToClient} from '../host/conversions.js';
import {maybeWrapWithLogging} from '../mojo_logging.js';

class InvalidArgumentError extends Error {}

const SILENT_SUCCESS: ChromeToolExecutionResult = {
  result: '{}',
  scheduling: ChromeToolResponseScheduling.SILENT,
};

function requireString(args: Record<string, unknown>, key: string): string {
  const val = args[key];
  if (typeof val !== 'string') {
    throw new InvalidArgumentError(
        `Missing or invalid string argument: ${key}`);
  }
  return val;
}

function optionalString(
    args: Record<string, unknown>, key: string, defaultValue = ''): string {
  const val = args[key];
  return typeof val === 'string' ? val : defaultValue;
}

function requireNumber(args: Record<string, unknown>, key: string): number {
  const val = Number(args[key]);
  if (isNaN(val)) {
    throw new InvalidArgumentError(
        `Missing or invalid number argument: ${key}`);
  }
  return val;
}

function requireBoolean(args: Record<string, unknown>, key: string): boolean {
  const val = args[key];
  if (typeof val !== 'boolean') {
    throw new InvalidArgumentError(
        `Missing or invalid boolean argument: ${key}`);
  }
  return val;
}

class GlicToolsHostImpl implements GlicToolsHost {
  constructor(private toolsRemote: AiOverlayToolsRemote) {}

  async getChromeTools(): Promise<ChromeTool[]|undefined> {
    const parsedDefs = JSON.parse(kBuiltInToolDefinitions) as Array<{
                         functionDeclarations?: Array<{
                                               name: string,
                                               description?: string,
                                               parameters?: unknown,
                                               behavior?: string,
                                             }>,
                       }>;

    const tools: ChromeTool[] = [];
    for (const group of parsedDefs) {
      for (const decl of group.functionDeclarations ?? []) {
        let blockingBehavior = ChromeToolBlockingBehavior.BLOCKING;
        if (decl.behavior === 'NON_BLOCKING') {
          blockingBehavior = ChromeToolBlockingBehavior.NON_BLOCKING;
        }
        tools.push({
          name: decl.name,
          description: decl.description ?? '',
          jsonSchemaParameters:
              decl.parameters ? JSON.stringify(decl.parameters) : '{}',
          blockingBehavior,
        });
      }
    }
    return tools;
  }

  async executeTool(toolName: string, jsonArguments: string):
      Promise<ChromeToolExecutionResult> {
    let args: Record<string, unknown> = {};
    if (jsonArguments && jsonArguments.trim() !== '') {
      try {
        args = JSON.parse(jsonArguments) as Record<string, unknown>;
      } catch {
        return {
          errorReason: ExecuteToolErrorReason.INVALID_ARGUMENTS,
          scheduling: ChromeToolResponseScheduling.UNSPECIFIED,
        };
      }
    }

    try {
      switch (toolName) {
        case 'open_url':
          await this.toolsRemote.openUrl(
              requireString(args, 'url'), requireBoolean(args, 'new_tab'));
          return SILENT_SUCCESS;

        case 'follow_link':
          await this.toolsRemote.followLink(requireString(args, 'id'));
          return SILENT_SUCCESS;

        case 'perform_search':
          await this.toolsRemote.performSearch(
              requireString(args, 'query'), requireBoolean(args, 'new_tab'));
          return SILENT_SUCCESS;

        case 'switch_tab': {
          const resp =
              await this.toolsRemote.switchTab(requireString(args, 'query'));
          return {
            result: JSON.stringify(resp ?? {}),
            scheduling: ChromeToolResponseScheduling.SILENT,
          };
        }

        case 'close_current_tab':
          await this.toolsRemote.closeCurrentTab();
          return SILENT_SUCCESS;

        case 'go_back':
          await this.toolsRemote.goBack();
          return SILENT_SUCCESS;

        case 'go_forward':
          await this.toolsRemote.goForward();
          return SILENT_SUCCESS;

        case 'reload_page':
          await this.toolsRemote.reloadPage();
          return SILENT_SUCCESS;

        case 'find_and_highlight':
          await this.toolsRemote.findAndHighlight(requireString(args, 'query'));
          return SILENT_SUCCESS;

        case 'scroll': {
          const granularity = args['granularity'] === 'document' ?
              ScrollGranularity.kDocument :
              ScrollGranularity.kPage;
          await this.toolsRemote.scroll(
              granularity, requireNumber(args, 'magnitude'));
          return SILENT_SUCCESS;
        }

        case 'play_video':
          await this.toolsRemote.playVideo();
          return SILENT_SUCCESS;

        case 'pause_video':
          await this.toolsRemote.pauseVideo();
          return SILENT_SUCCESS;

        case 'seek_to_timestamp':
          await this.toolsRemote.seekToTimestamp(
              requireString(args, 'timecode'));
          return SILENT_SUCCESS;

        case 'translate_page':
          await this.toolsRemote.translatePage(
              optionalString(args, 'target_language'));
          return SILENT_SUCCESS;

        case 'add_bookmark':
          await this.toolsRemote.addBookmark();
          return SILENT_SUCCESS;

        case 'remove_bookmark':
          await this.toolsRemote.removeBookmark();
          return SILENT_SUCCESS;

        case 'open_page': {
          const resp =
              await this.toolsRemote.openPage(requireString(args, 'query'));
          return {
            result: resp,
            scheduling: ChromeToolResponseScheduling.SILENT,
          };
        }

        case 'set_text':
          await this.toolsRemote.setText(
              {value: requireNumber(args, 'dom_node_id')},
              requireString(args, 'text'));
          return SILENT_SUCCESS;

        case 'click_element':
          await this.toolsRemote.clickElement(
              {value: requireNumber(args, 'dom_node_id')});
          return SILENT_SUCCESS;

        case 'set_fullscreen':
          await this.toolsRemote.setFullscreen(
              requireBoolean(args, 'fullscreen'));
          return SILENT_SUCCESS;

        default:
          return {
            errorReason: ExecuteToolErrorReason.TOOL_NOT_FOUND,
            scheduling: ChromeToolResponseScheduling.UNSPECIFIED,
          };
      }
    } catch (e) {
      if (e instanceof InvalidArgumentError) {
        return {
          errorReason: ExecuteToolErrorReason.INVALID_ARGUMENTS,
          scheduling: ChromeToolResponseScheduling.UNSPECIFIED,
        };
      }
      console.error('Failed to execute tool:', toolName, e);
      return {
        errorReason: ExecuteToolErrorReason.EXECUTION_FAILED,
        scheduling: ChromeToolResponseScheduling.UNSPECIFIED,
      };
    }
  }
}

export class GlicBrowserHostTools implements Partial<GlicBrowserHost> {
  private toolsRemote?: AiOverlayToolsRemote;
  private toolsInstance?: GlicToolsHost;

  initialize(
      initialState: WebClientInitialState, handler?: WebClientHandlerRemote) {
    if (hostCapabilitiesToClient(initialState.hostCapabilities)
            .includes(HostCapability.CHROME_TOOLS) &&
        handler) {
      const toolsRemote = new AiOverlayToolsRemote();
      this.toolsRemote =
          maybeWrapWithLogging(toolsRemote, {prefix: 'AiOverlayToolsRemote'});
      handler.createAiOverlayTools(toolsRemote.$.bindNewPipeAndPassReceiver());
      this.toolsInstance = new GlicToolsHostImpl(this.toolsRemote);
    } else {
      this.tools = undefined;
    }
  }

  destroyTools(): void {
    if (this.toolsRemote) {
      this.toolsRemote.$.close();
      this.toolsRemote = undefined;
    }
    this.toolsInstance = undefined;
    this.tools = undefined;
  }

  tools?(): GlicToolsHost {
    assert(this.toolsInstance);
    return this.toolsInstance;
  }
}
