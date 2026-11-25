// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';

import {kBuiltInToolDefinitions} from '../../generated_tool_definitions.js';
import type {WebClientHandlerRemote, WebClientInitialState} from '../../glic.mojom-webui.js';
import {                         //
  ChromeToolBlockingBehavior,    //
  ChromeToolResponseScheduling,  //
  ExecuteToolErrorReason,        //
  HostCapability,                //
} from '../../glic_api/glic_api.js';
import type {ChromeTool, ChromeToolExecutionResult, GlicBrowserHost, GlicToolsHost} from '../../glic_api/glic_api.js';
import {AiOverlayToolsRemote, ScrollGranularity} from '../../tools.mojom-webui.js';
import {hostCapabilitiesToClient} from '../conversions.js';
import {wrapMojoRemote} from '../mojo_remote.js';

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
  constructor(
      private isAvailable: () => boolean,
      private getToolsRemote: () => AiOverlayToolsRemote) {}

  async getChromeTools(): Promise<ChromeTool[]|undefined> {
    if (!this.isAvailable()) {
      return undefined;
    }
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
        const parsed: unknown = JSON.parse(jsonArguments);
        if (typeof parsed !== 'object' || parsed === null ||
            Array.isArray(parsed)) {
          return {
            errorReason: ExecuteToolErrorReason.INVALID_ARGUMENTS,
            scheduling: ChromeToolResponseScheduling.UNSPECIFIED,
          };
        }
        args = parsed as Record<string, unknown>;
      } catch {
        return {
          errorReason: ExecuteToolErrorReason.INVALID_ARGUMENTS,
          scheduling: ChromeToolResponseScheduling.UNSPECIFIED,
        };
      }
    }

    try {
      switch (toolName) {
        case 'open_url': {
          const url = requireString(args, 'url');
          const newTab = requireBoolean(args, 'new_tab');
          await this.getToolsRemote().openUrl(url, newTab);
          return SILENT_SUCCESS;
        }

        case 'follow_link': {
          const id = requireString(args, 'id');
          await this.getToolsRemote().followLink(id);
          return SILENT_SUCCESS;
        }

        case 'perform_search': {
          const query = requireString(args, 'query');
          const newTab = requireBoolean(args, 'new_tab');
          await this.getToolsRemote().performSearch(query, newTab);
          return SILENT_SUCCESS;
        }

        case 'switch_tab': {
          const query = requireString(args, 'query');
          const resp = await this.getToolsRemote().switchTab(query);
          return {
            result: JSON.stringify(resp ?? {}),
            scheduling: ChromeToolResponseScheduling.SILENT,
          };
        }

        case 'close_current_tab':
          await this.getToolsRemote().closeCurrentTab();
          return SILENT_SUCCESS;

        case 'go_back':
          await this.getToolsRemote().goBack();
          return SILENT_SUCCESS;

        case 'go_forward':
          await this.getToolsRemote().goForward();
          return SILENT_SUCCESS;

        case 'reload_page':
          await this.getToolsRemote().reloadPage();
          return SILENT_SUCCESS;

        case 'find_and_highlight': {
          const query = requireString(args, 'query');
          await this.getToolsRemote().findAndHighlight(query);
          return SILENT_SUCCESS;
        }

        case 'scroll': {
          const granularity = args['granularity'] === 'document' ?
              ScrollGranularity.kDocument :
              ScrollGranularity.kPage;
          const magnitude = requireNumber(args, 'magnitude');
          await this.getToolsRemote().scroll(granularity, magnitude);
          return SILENT_SUCCESS;
        }

        case 'play_video':
          await this.getToolsRemote().playVideo();
          return SILENT_SUCCESS;

        case 'pause_video':
          await this.getToolsRemote().pauseVideo();
          return SILENT_SUCCESS;

        case 'seek_to_timestamp': {
          const timecode = requireString(args, 'timecode');
          await this.getToolsRemote().seekToTimestamp(timecode);
          return SILENT_SUCCESS;
        }

        case 'translate_page': {
          const targetLanguage = optionalString(args, 'target_language');
          await this.getToolsRemote().translatePage(targetLanguage);
          return SILENT_SUCCESS;
        }

        case 'add_bookmark':
          await this.getToolsRemote().addBookmark();
          return SILENT_SUCCESS;

        case 'remove_bookmark':
          await this.getToolsRemote().removeBookmark();
          return SILENT_SUCCESS;

        case 'open_page': {
          const query = requireString(args, 'query');
          const resp = await this.getToolsRemote().openPage(query);
          return {
            result: resp,
            scheduling: ChromeToolResponseScheduling.SILENT,
          };
        }

        case 'set_text': {
          const domNodeId = requireNumber(args, 'dom_node_id');
          const text = requireString(args, 'text');
          await this.getToolsRemote().setText({value: domNodeId}, text);
          return SILENT_SUCCESS;
        }

        case 'click_element': {
          const domNodeId = requireNumber(args, 'dom_node_id');
          await this.getToolsRemote().clickElement({value: domNodeId});
          return SILENT_SUCCESS;
        }

        case 'set_fullscreen': {
          const fullscreen = requireBoolean(args, 'fullscreen');
          await this.getToolsRemote().setFullscreen(fullscreen);
          return SILENT_SUCCESS;
        }

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
  private handler?: WebClientHandlerRemote;
  private toolsRemote?: AiOverlayToolsRemote;
  private readonly toolsInstance: GlicToolsHost = new GlicToolsHostImpl(
      () => Boolean(this.handler), () => this.getOrCreateToolsRemote());

  initialize(
      initialState: WebClientInitialState, handler?: WebClientHandlerRemote) {
    if (!hostCapabilitiesToClient(initialState.hostCapabilities)
             .includes(HostCapability.CHROME_TOOLS) ||
        !handler) {
      this.tools = undefined;
      return;
    }
    this.handler = handler;
  }

  tools?(): GlicToolsHost {
    return this.toolsInstance;
  }

  private getOrCreateToolsRemote(): AiOverlayToolsRemote {
    if (this.toolsRemote) {
      return this.toolsRemote;
    }
    assert(this.handler);
    const toolsRemote = new AiOverlayToolsRemote();
    this.toolsRemote = wrapMojoRemote(
        toolsRemote, {prefix: 'AiOverlayToolsRemote', hangOnDisconnect: true});
    this.handler.createAiOverlayTools(
        toolsRemote.$.bindNewPipeAndPassReceiver());
    return this.toolsRemote;
  }
}
