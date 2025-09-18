# Applying Gemini CLI to Fix Chromium Unsafe Buffer Usage

## Background

This prompt task applies Gemini CLI to identify and fix unsafe buffer usage in
the Chromium codebase.

For more details, see [/docs/unsafe_buffers.md](/docs/unsafe_buffers.md).

Googler-only docs:
[go/gemini-spanification-setup](http://go/gemini-spanification-setup)

## Setup

1. Setup Gemini CLI following [/agents/README.md](/agents/README.md).
2. Create //GEMINI.md following
   [/agents/prompts/README.md](/agents/prompts/README.md).
3. landmines extension is recommended. See
   [/agents/extensions/README.md](/agents/extensions/README.md).

## Usage

```bash
vpython3 agents/prompts/projects/spanification/run.py [DIR|FILE_PATH]
```
