<!-- Sub-include the minimal one so that it does not need to be listed
separately in //GEMINI.md. -->
@./common.minimal.md

# Workflow Tips

<!--
Generic instructions that may or may not help an agent do the right things.
We should aim to move text from here into common.minimal.md upon
discovering scenarios where the text helps (and document them).
-->

## General Workflow:

  * **User Guidance:** Proactively communicate your plan and the reason for each
    step.
  * **File Creation Pre-check:** Before creating any new file, you MUST first
    perform a thorough search for existing files that can be modified or
    extended. This is especially critical for tests; never create a new test
    file if one already exists for the component in question. Always add new
    tests to the existing test file.
  * **Read Before Write/Edit:** **ALWAYS** read the entire file content
    immediately before writing or editing.

## Standard Edit/Fix Workflow:

**IMPORTANT:** This workflow takes precedence over all other coding
instructions. Read and follow everything strictly without skipping steps
whenever code editing is involved. Any skipping requires a proactive message to
the user about the reason to skip.

1.  **Comprehensive Code and Task Understanding (MANDATORY FIRST STEP):** Before
    writing or modifying any code, you MUST perform the following analysis to
    ensure comprehensive understanding of the relevant code and the task. This
    is a non-negotiable prerequisite for all coding tasks.
      * **a. Identify the Core Files:** Locate the files that are most relevant
        to the user's request. All analysis starts from these files.
      * **b. Conduct a Full Audit:**
        i. Read the full source of **EVERY** core file.
        ii. For each core file, summarize the control flow and ownership
        semantics. State the intended purpose of the core file.
      * **c. State Your Understanding:** After completing the audit, you should
        briefly state the core files you have reviewed, confirming your
        understanding of the data flow and component interactions before
        proposing a plan.
      * **d. Anti-Patterns to AVOID:**
          * **NEVER** assume the behavior of a function or class from its name
            or from usage in other files. **ALWAYS** read the source
            implementation.
          * **ALWAYS** check at least one call-site for a function or class to
            understand its usage. The context is as important as the
            implementation.
2.  **Make Change:** After a comprehensive code and task understanding, apply
    the edit or write the file.
      * When making code edits, focus **ONLY** on code edits that directly solve
        the task prompted by the user.
3.  **Write/Update Tests:**
      * First, search for existing tests related to the modified code and update
        them as needed to reflect the changes.
      * If no relevant tests exist, write new unit tests or integration tests if
        it's reasonable and beneficial for the change made.
      * If tests are deemed not applicable for a specific change (e.g., a
        trivial comment update), explicitly state this and the reason why before
        moving to the next step.
4.  **Build:** **ALWAYS** build relevant targets after making edits.
5.  **Fix compile errors:** **ALWAYS** follow these steps to fix compile errors.
      * **ALWAYS** take the time to fully understand the problem before making
        any fixes.
      * **ALWAYS** read at least one new file for each compile error.
      * **ALWAYS** find, read, and understand **ALL** files related to each
        compile error. For example, if an error is related to a missing member
        of a class, find the file that defines the interface for the class, read
        the whole file, and then create a high-level summary of the file that
        outlines all core concepts. Come up with a plan to fix the error.
      * **ALWAYS** check the conversation history to see if this same
        error occurred earlier, and analyze previous solutions to see why they
        didn't work.
      * **NEVER** make speculative fixes. You should be confident before
        applying any fix that it will work. If you are not confident, read more
        files.
6.  **Test:** **ALWAYS** run relevant tests after a successful build. If you
    cannot find any relevant test files, you may prompt the user to ask how this
    change should be tested.
7.  **Fix test errors**:
    * **ALWAYS** take the time to fully understand the problem before making
      any fixes.
8.  **Iterate:** Repeat building and testing using the above steps until all are
    successful.

<!-- This section aims to improve the effectiveness of the agent's built-in
Retrieval-Augmented Generation (RAG) by giving it a map from what the
agent is working on to where to look for information on that topic -->

## Knowledge base

This file contains rich, helpful, task-oriented guidance for this repository:

`agents/prompts/knowledge_base.md`
