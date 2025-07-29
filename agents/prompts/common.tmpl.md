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

## \!\! MANDATORY DEBUGGING PROTOCOL (WHEN STUCK) \!\!

  * **Trigger:** You **MUST** activate this protocol if you encounter a
    **Repeated Tool or Command Failure**.

      * **Definition of Repeated Failure:** A tool or command (e.g.,
        `autoninja`, `autotest.py`, `git cl format`, `replace`) fails. You apply
        a fix or change your approach. You run the *exact same tool or command*
        again, and it fails for a **second time**.
      * **Sensitivity:** This protocol is intentionally highly sensitive. The
        error message for the second failure does **NOT** need to be the same as
        the first. Any subsequent failure of the same tool or command after a
        fix attempt is a trigger. This is to prevent "whack-a-mole" scenarios
        where fixing one error simply reveals another, indicating a deeper
        underlying problem.

    *Check your history to confirm the repeated failure of the tool or command.*

  * **Action:** If the trigger condition is met:

    1.  **STOP:** **DO NOT** immediately retry the *same* fix or re-run the
        *same* tool or command again.
    2.  **INFORM USER:** Immediately inform the user that you are invoking the
        debugging protocol because a tool or command has failed twice in a row.
    3.  **REASON:** **Explicitly state** which tool or command failed repeatedly
        (e.g., "`autotest` failed, I applied a fix, and it failed again. I am
        now invoking the debugging protocol to analyze the root cause.").
        Mentioning the specific error messages is good, but the repeated failure
        is the primary trigger.
    4.  **DEBUG:** Look closely into your own context, memory, and traces. Give
        a deep analysis of why you are repeating mistakes and stuck in a failure
        loop. The analysis should focus on the *root cause* of the repeated
        failures, not just the most recent error message. Utilize any tools that
        help with the debugging investigation.
    5.  **PROCEED:** Use the suggestions returned by the DEBUG step to inform
        your next attempt at a fix. Explain the new, more comprehensive plan to
        the user. If the DEBUG step provides tool calls, execute them.
        Otherwise, formulate a new plan based on its suggestions.

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
9.  **Format:** Before finishing the task, **ALWAYS** run `git cl format` to
    ensure the new changes are formatted properly.
