// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ScrollGranularity} from '../tools.mojom-webui.js';
import type {AiOverlayToolsRemote} from '../tools.mojom-webui.js';

import {kBuiltInToolDefinitions} from './../generated_tool_definitions.js';

interface OpenUrlArgs {
  url: string;
  new_tab: boolean;
}

interface PerformSearchArgs {
  query: string;
  new_tab: boolean;
}

interface SwitchTabArgs {
  query: string;
}

interface FindAndHighlightArgs {
  query: string;
}

interface ScrollArgs {
  direction: 'up'|'down'|'top'|'bottom';
}

interface SeekToTimestampArgs {
  timecode: string;
}
type ToolCall =|{name: 'open_url', args: OpenUrlArgs}|
    {name: 'perform_search', args: PerformSearchArgs}|
    {name: 'switch_tab', args: SwitchTabArgs}|
    {name: 'find_and_highlight', args: FindAndHighlightArgs}|
    {name: 'scroll', args: ScrollArgs}|
    {name: 'seek_to_timestamp', args: SeekToTimestampArgs}|
    {name: 'close_current_tab', args: Record<string, unknown>}|
    {name: 'go_back', args: Record<string, unknown>}|
    {name: 'go_forward', args: Record<string, unknown>}|
    {name: 'reload_page', args: Record<string, unknown>}|
    {name: 'play_video', args: Record<string, unknown>}|
    {name: 'pause_video', args: Record<string, unknown>};

export class ToolExecutor {
  private readonly toolsRemote: AiOverlayToolsRemote;

  constructor(toolsRemote: AiOverlayToolsRemote) {
    this.toolsRemote = toolsRemote;
  }

  getToolDefinitions(): string {
    return kBuiltInToolDefinitions;
  }

  async executeTool(name: string, args: Record<string, unknown>):
      Promise<Record<string, unknown>> {
    try {
      const outResult:
          Record<string, unknown> = {success: true, scheduling: 'SILENT'};

      const call = {name, args} as unknown as ToolCall;

      switch (call.name) {
        case 'open_url': {
          await this.toolsRemote.openUrl(call.args.url, call.args.new_tab);
          break;
        }
        case 'perform_search': {
          await this.toolsRemote.performSearch(
              call.args.query, call.args.new_tab);
          break;
        }
        case 'switch_tab': {
          // Mojo result<T, string> bindings resolve to T on success and reject
          // with string on error.
          const switchTabResult =
              await this.toolsRemote.switchTab(call.args.query);
          Object.assign(outResult, switchTabResult);
          break;
        }
        case 'close_current_tab':
          await this.toolsRemote.closeCurrentTab();
          break;
        case 'go_back':
          await this.toolsRemote.goBack();
          break;
        case 'go_forward':
          await this.toolsRemote.goForward();
          break;
        case 'reload_page':
          await this.toolsRemote.reloadPage();
          break;
        case 'find_and_highlight': {
          await this.toolsRemote.findAndHighlight(call.args.query);
          break;
        }
        case 'scroll':
          await this.execScroll(call.args);
          break;
        case 'play_video':
          await this.toolsRemote.playVideo();
          break;
        case 'pause_video':
          await this.toolsRemote.pauseVideo();
          break;
        case 'seek_to_timestamp': {
          await this.toolsRemote.seekToTimestamp(call.args.timecode);
          break;
        }
        default:
          return {success: false, error: `Unknown tool: ${name}`};
      }

      return outResult;
    } catch (e) {
      console.error(`Error executing tool ${name}:`, e);
      return {success: false, error: String(e)};
    }
  }

  private async execScroll(args: ScrollArgs): Promise<void> {
    let granularity: ScrollGranularity;
    let magnitude: number;

    switch (args.direction) {
      case 'up':
        granularity = ScrollGranularity.kPage;
        magnitude = -1;
        break;
      case 'down':
        granularity = ScrollGranularity.kPage;
        magnitude = 1;
        break;
      case 'top':
        granularity = ScrollGranularity.kDocument;
        magnitude = -1;
        break;
      case 'bottom':
        granularity = ScrollGranularity.kDocument;
        magnitude = 1;
        break;
      default:
        throw new Error(`Unknown scroll direction: ${args.direction}`);
    }
    await this.toolsRemote.scroll(granularity, magnitude);
  }
}
