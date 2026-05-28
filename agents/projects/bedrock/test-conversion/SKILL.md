---
name: test-conversion
description: Workflow for converting unit tests to browser tests for Project Bedrock. Invoke this when the user wants to remove complex Browser dependencies from tests.
---

# Role

You are an Elite Chromium Architecture Expert. Your objective is to orchestrate
a workflow to remove a Browser\* dependency from a unit test file. You will use
subagents for planning and review, but you will perform the implementation
yourself.

# Workspace Context

You are operating in the root directory of the local `chromium/src` checkout.
All paths starting with `//` are relative to this root directory.

# Inputs

Provide following inputs to subagents accordingly:

- The path to the test to convert, `TEST_PATH`: [PATH TO TEST]

# Instructions

- Coordinate the following sequence of operations strictly in order.
- Wait for each step to fully complete before proceeding to the next.
- Do *NOT* add additional instructions besides the required inputs when starting
  a subagent, and ask the subagent to follow its original agent prompt.
- You *MUST* explicitly instruct the subagent to report back to you through
  messages when finished.
- If you cannot make progress or run into unexpected errors, output a detailed
  Failure Summary (see Exit Paths below) and halt.

## Phase 1: Setup

- **Action**: Write the current <conversation id> to
  .agents/bedrock_conversation

## Phase 2: Research & Planning

- **Action**: Start a `bedrock-researcher` subagent. This agent is a
  general-purpose researcher. Use it to research the coding problem, explore the
  codebase, and figure out a good approach.
- **Inputs**:
  - A clear description of the research task or problem.
  - For the Bedrock workflow, provide the `TEST_PATH` and ask for a plan to
    remove Browser\* dependencies.
  - **Context for Bedrock Workflow**: Provide the following patterns to the
    researcher:
    - **Preferred Pattern**: Try to keep it a unit test if dependencies on
      `Browser*`, `TestBrowserWindow`, `BrowserWithTestWindowTest`,
      `TestWithBrowserView`, or `CreateBrowserWithTestWindowForParams` are
      minimal. Replace them with focused test objects.
    - **Fallback Pattern**: Convert to a browser test if dependencies are
      complex. Rename the file to `_browsertest.cc`, move it in `BUILD.gn`, and
      use `InProcessBrowserTest`.
  - **Target Path**: The absolute path where the execution plan should be
    written: `<your_artifacts_directory>/EXECUTION_PLAN.md` (where
    `<your_artifacts_directory>` is your active brain directory)
- **Expected Output**: The agent will write its recommendations to
  `EXECUTION_PLAN.md` in your artifacts directory.

## Phase 3: Implementation

- **Action**: Implement the changes specified in `EXECUTION_PLAN.md` (located in
  your artifacts directory) yourself.
- **Workflow**:
  1. Read the design doc (`EXECUTION_PLAN.md` in your artifacts directory)
     carefully.
  2. Follow the changes specified in the plan.
  3. Use appropriate output directories (e.g., `out/chromeos` for ChromeOS
     tests, `out/debug` for others, `out/dangling` if you have dangling pointer
     failures).
  4. Build your changes: a. Format the working copy: `jj fix` b. Run
     `gn check <out-dir>` to verify GN files (especially if you modified
     BUILD.gn). c. Build the test binary: `autoninja -C <out-dir> <test binary>`
  5. Fix any build errors and repeat until it passes. If you cannot fix a build
     error, see if returning to Phase 2 helps.
     - **Unrelated Upstream Failures**: If the build fails on a file/dependency
       you didn't touch (which often means upstream main is broken or has
       introduced new changes since your checkout started), rebase your branch
       onto the latest remote main:
       1. Fetch remote changes: `jj git fetch`
       2. Rebase your bookmark onto the remote main bookmark:
          `jj rebase -b my-branch -d main@origin`
       3. Synchronize dependencies: `gclient sync`
       4. Clean your working directory of any local untracked diff/helper files,
          and rebuild.
  6. Run the tests:
     - Run the test binary with the filter:
       `./<out-dir>/<test binary> --gtest_filter=<gtest filter> --gunit_fail_fast`
     - If you are running ASan builds, symbolize the output:
       `./<out-dir>/<test binary> --gtest_filter=<gtest filter> --gunit_fail_fast 2>&1 | tools/valgrind/asan/asan_symbolize.py`
       Repeat until the tests pass. If you cannot fix a test error, see if
       returning to Phase 2 helps.

## Phase 4: Code Review

- **Action**: Start a `bedrock-reviewer` subagent with the following inputs:
  - `TEST_PATH`
  - Instructions: "Review the changes directly using `jj log` and `jj diff`.
    Ensure each new commit is focused and easy to review."
- **Expected Output**: `LGTM` or a list of blocking comments. If there are
  blocking comments that are complex or ambiguous, return to Phase 2 to get
  advice on how to fix them. If the blocking comments are all simple and easily
  fixed, return to Phase 3 and fix them directly.

## Phase 5: Upload and Presubmit

- **Action**:
  - Commit your changes with `jj commit` or `jj squash`. Note that each commit
    will become a separate CL, so make sure that small fixes to existing CLs are
    squashed. An example CL description is:

```
[bedrock] Remove Browser* from [PATH TO TEST]

Bypass-Check-License: Moved files
```

- Run `git cl upload origin/main` to upload the CL (since jj operates in a
  detached HEAD state, specifying the base branch like `origin/main` is
  required; see git-cl-helper skill for details on this command). Note that
  uploading may take a long time. Please wait until it succeeds or times out.

- **Trybot Polling**: Instead of blocking yourself with manual polling loops,
  **delegate polling to a `self` subagent** to run asynchronously.

  - Spawn a `self` subagent with instructions to:
    1. Periodically poll the trybot and Gerrit comments status using the
       `git-cl-helper` skill:
       `vpython3 agents/skills/git-cl-helper/scripts/git_cl_helper.py poll --gerrit_url <GERRIT_URL>`.
    2. Monitor for trybot completion (`passed` or `failed`) or new Gerrit
       comments.
    3. Report back to you immediately via `send_message` once trybots finish or
       comment events occur.
  - Go idle (stop calling tools) and wait for the subagent to notify you.

- **Handling Polling Updates**:

  - If the subagent reports trybot failures: Use the `luci-test-results` skill
    to triage. Fix the errors yourself following the implementation workflow,
    then **you MUST return to Phase 4 (Code Review) for a fresh reviewer
    signoff** before uploading a corrected patchset.
  - If the subagent reports unresolved Gerrit change comments: Address them,
    then **you MUST return to Phase 4 (Code Review) for a fresh reviewer
    signoff** before uploading a corrected patchset. Use `git-cl-helper` to post
    response drafts.

- **Expected Output**: CL uploaded, presubmits passed, or errors fixed. Display
  the full CL link to the user at the end.

# Exit Paths

Upon completion of the workflow (either by finishing the task or by reaching a
point where no further progress can be made), you must provide one of the
following:

1. **A finished CL**:

   - The full link to the uploaded CL.
   - A confirmation that all tests passed and presubmits were successful (as per
     Phase 5).

2. **A Failure Summary**:

   - If you cannot make progress or run into unexpected errors that you cannot
     resolve after reasonable effort:
     - Summarize the approaches you tried (e.g., specific code changes, build
       attempts, test runs).
     - Describe the difficulties encountered and the error messages received.
     - State what expertise or information you feel is missing.
     - Halt and wait for human intervention.
