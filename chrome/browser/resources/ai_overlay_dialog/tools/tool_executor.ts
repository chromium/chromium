// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {AiOverlayToolsRemote} from '../tools.mojom-webui.js';

import {kBuiltInToolDefinitions} from './../generated_tool_definitions.js';

export class ToolExecutor {
  private readonly toolsRemote: AiOverlayToolsRemote;

  constructor(toolsRemote: AiOverlayToolsRemote) {
    this.toolsRemote = toolsRemote;
  }

  getToolDefinitions(): string {
    return kBuiltInToolDefinitions;
  }

  async executeTool(name: string, args: any): Promise<any> {
    try {
      const outResult: any = {success: true, scheduling: 'SILENT'};

      switch (name) {
        case 'open_url':
          await this.toolsRemote.openUrl(args.url, args.new_tab);
          break;
        case 'perform_search':
          await this.toolsRemote.performSearch(args.query, args.new_tab);
          break;
        case 'switch_tab': {
          // Mojo result<T, string> bindings resolve to T on success and reject
          // with string on error.
          const switchTabResult = await this.toolsRemote.switchTab(args.query);
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
        case 'find_and_highlight':
          await this.toolsRemote.findAndHighlight(args.query);
          break;
        case 'scroll':
          await this.toolsRemote.scroll(args.direction, args.magnitude);
          break;
        case 'play_video':
          await this.toolsRemote.playVideo();
          break;
        case 'pause_video':
          await this.toolsRemote.pauseVideo();
          break;
        case 'seek_to_timestamp':
          await this.toolsRemote.seekToTimestamp(args.timecode);
          break;
        default:
          return {success: false, error: `Unknown tool: ${name}`};
      }

      return outResult;
    } catch (e) {
      console.error(`Error executing tool ${name}:`, e);
      return {success: false, error: String(e)};
    }
  }
}
