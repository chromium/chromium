// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';

import type {Journal} from '../journal.js';
import type {PageContextManager} from '../page_context_manager.js';
import {formatPageVisitHistory, formatTranscript} from '../persona.js';
import {ScrollGranularity} from '../tools.mojom-webui.js';
import type {AiOverlayToolsRemote} from '../tools.mojom-webui.js';

import {kBuiltInToolDefinitions} from './../generated_tool_definitions.js';

interface OpenUrlArgs {
  url: string;
  new_tab: boolean;
}

interface FollowLinkArgs {
  id: string;
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
  granularity: string;
  magnitude: number;
}

interface SeekToTimestampArgs {
  timecode: string;
}

interface TranslatePageArgs {
  target_language: string;
}

interface InvokeGlicArgs {
  prompt: string;
}

type ToolCall =|{name: 'open_url', args: OpenUrlArgs}|
    {name: 'follow_link', args: FollowLinkArgs}|
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
    {name: 'pause_video', args: Record<string, unknown>}|
    {name: 'translate_page', args: TranslatePageArgs}|
    {name: 'get_page_content', args: Record<string, unknown>}|
    {name: 'get_session_history', args: Record<string, unknown>}|
    {name: 'invoke_glic', args: InvokeGlicArgs};

export class ToolExecutor {
  private readonly toolsRemote: AiOverlayToolsRemote;
  private readonly pageContextManager: PageContextManager;
  private readonly journal: Journal;
  private readonly personaName: string;

  constructor(
      toolsRemote: AiOverlayToolsRemote, pageContextManager: PageContextManager,
      journal: Journal, personaName: string) {
    this.toolsRemote = toolsRemote;
    this.pageContextManager = pageContextManager;
    this.journal = journal;
    this.personaName = personaName;
  }

  getToolDefinitions(): string {
    const tools = JSON.parse(kBuiltInToolDefinitions);
    assert(tools.length > 0 && tools[0].functionDeclarations);
    tools[0].functionDeclarations.push(
        {
          name: 'get_page_content',
          description: 'Use `get_page_content` if you need the ' +
              'full text of the current page.',
        },
        {
          name: 'get_session_history',
          description: 'Use `get_session_history` to recall ' +
              'details from earlier in the session.',
        });
    return JSON.stringify(tools);
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
        case 'follow_link': {
          await this.toolsRemote.followLink(call.args.id);
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
        case 'translate_page': {
          await this.toolsRemote.translatePage(call.args.target_language);
          break;
        }
        case 'get_page_content': {
          outResult['content'] = this.pageContextManager.pageContext?.content ||
              'No content available for this page.';
          break;
        }
        case 'get_session_history': {
          const turns = this.journal.getTurnEntries();
          const pages = this.journal.getPageVisitEntries();
          const transcript = formatTranscript(turns, this.personaName);
          const pageHistory = formatPageVisitHistory(pages);
          outResult['history'] = `## Pages Visited\n${pageHistory}\n\n` +
              `## Conversation History\n${transcript}`;
          break;
        }
        case 'invoke_glic': {
          const glicResult =
              await this.toolsRemote.invokeGlic(call.args.prompt);
          if (typeof glicResult === 'string') {
            return {success: false, error: glicResult};
          }
          if (!glicResult || typeof glicResult !== 'object' ||
              !('value' in glicResult)) {
            return {success: false, error: 'Unknown result'};
          }
          outResult['response'] = (glicResult as {value: string}).value;
          outResult['scheduling'] = 'INTERRUPT';
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
    // TODO(b/513218774): Ideally generated_tool_definitions would also
    // generate a validator function for each tool to validate the incoming
    // Args objects so we could do this automatically for each tool.
    if (typeof args !== 'object' || typeof args.magnitude !== 'number' ||
        (args.granularity !== 'document' && args.granularity !== 'page')) {
      throw new Error(`Bad Scroll Args: ${JSON.stringify(args)}`);
    }

    const granularity = args.granularity === 'document' ?
        ScrollGranularity.kDocument :
        ScrollGranularity.kPage;

    await this.toolsRemote.scroll(granularity, args.magnitude);
  }
}
