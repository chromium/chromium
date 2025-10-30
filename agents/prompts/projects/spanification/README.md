# Applying Gemini CLI to Fix Chromium Unsafe Buffer Usage

## CodeHealth rotation

This script and prompt are powering the CodeHealth rotation for spanification of
unsafe buffer usage in Chromium. See [go/code-health-unsafe-buffer-access](https://docs.google.com/document/d/1CSSBJLjDdcLhiat67mFO-2OHxuXdSXdDiJa1k8_06DM/edit?tab=t.0) and the [list of bugs](https://issues.chromium.org/issues/435317390/dependencies).

The pending generation, patches, and bugs are tracked in the following
spreadsheet: [go/codehealth-spanification-spreadsheet](https://goto.google.com/codehealth-spanification-spreadsheet)
(Googler-only)

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
vpython3 agents/prompts/projects/spanification/run.py [file_path]
```

Where `[file_path]` is the path to the Chromium source file you want to
process. The script will analyze the file, identify unsafe buffer usages,
generate spanified code.

The file `./gemini_spanification_output.json` will contains the commit message
and the logs of the operations.
