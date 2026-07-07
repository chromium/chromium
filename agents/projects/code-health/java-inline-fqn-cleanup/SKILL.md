---
name: java-inline-fqn-cleanup
description: Identify and clean up inline fully qualified Java names (FQNs) in first-party code, replacing them with standard imports.
---

# Code Health: Java Inline FQN Cleanup

Clean up inline fully qualified names (FQNs) in Java code by replacing them with
proper import statements at the top of the file, and then formatting/optimizing
the import order.

## Overview

Using fully qualified class names inline (e.g. `android.view.View view = ...`)
instead of importing them makes code harder to read and breaks standard style
conventions. This skill helps automate the discovery, safety analysis, and
clean-up of these inline qualifiers in Chromium's first-party Java files.

**Goal:** Clean up inline fully qualified names (FQNs) in first-party Java files
and replace them with standard imports.

## Relevant Resources & Style Guides

- [Chromium Java Style Guide - Imports](https://chromium.googlesource.com/chromium/src/+/main/styleguide/java/java.md#imports)
- **Implementation Patterns:** [patterns.md](references/patterns.md)
- **Discovery Script:** [find_candidates.py](scripts/find_candidates.py)
- **Automated Review Protocol:**
  [automated_review.md](references/automated_review.md)

## Workflow

> [!IMPORTANT] **Execution Protocol:** Execute all steps sequentially one by
> one. Do not skip any step. Do not use `edit-code` or `grep`. Use `rg`
> (ripgrep) for searches.

### Step 1: Workspace Preparation

Follow the workspace preparation steps in
[workspace_preparation.md](../hub/references/workspace_preparation.md) to ensure
a clean and updated environment.

### Step 2: Discovery & Batch Selection

Follow the
[Discovery & Batch Selection](../hub/references/discovery_and_batch_selection.md)
workflow. When presenting the batch, include the **Imports Found** and **Banned
FQNs** details.

### Step 3: Refactoring & Implementation

**CRITICAL RULE:** Standard file editing tools (like `replace_file_content` or
`multi_replace_file_content`) must be used to apply the cleanups directly to the
Java files. **DO NOT** create, write, or execute any custom python or bash
scripts to perform the text replacements.

- **References:** Refer to [patterns.md](references/patterns.md) for concrete
  examples and exceptions (e.g., handling java.lang.\* and Local R classes)
  before applying replacements.

Process the candidates by handling them **one file at a time**, and applying
modifications inside each file **one FQN at a time** (rather than refactoring
all files or FQNs at once). This ensures stability and allows for precise
verification. For each file in the batch, apply the following cleanup rules:

1. **Avoid Name Collisions & Shadowing:** Check if the simple class name of the
   FQN is already imported, defined, or implicitly resolved in that file (e.g.,
   via wildcard imports, implicit package-private classes, or other imported
   annotations with the same name). If there is a collision or shadowing risk,
   **do not modify the line** (the FQN must remain inline).
2. **Banned FQNs:** If the discovery script outputted a `Banned FQNs` list,
   completely ignore those specific FQNs. They are either raw package paths or
   invalid classes, and attempting to import them will break the build.
3. **First-Party Code Only:** Do not modify any files in `third_party/` or
   auto-generated directories.
4. **Import Strategy for Static Members:** When cleaning up inline FQNs for
   static constants, properties (e.g. `TabProperties`), feature flags
   (`ChromeFeatureList`), or enums (`TabLaunchType`), **prefer importing the
   class itself rather than importing its members statically**.
   - *Incorrect (Static Import):*
     `import static org.chromium.chrome.browser.tasks.tab_management.TabProperties.IS_SELECTED;`
   - *Correct (Class Import):*
     `import org.chromium.chrome.browser.tasks.tab_management.TabProperties;`
     and use `TabProperties.IS_SELECTED` in the code.
   - *Rationale:* Qualifying static constants (e.g. `TabProperties.TITLE`) keeps
     clear context of where the constant is defined and avoids namespace
     conflicts when multiple imported properties classes share the same field
     names (e.g. `TabProperties.TITLE` vs `FolderProperties.TITLE`).
5. **Clean up Javadocs & Comments:** Replace inline FQNs found inside code
   comments and Javadoc links (e.g. `{@link android.webkit.WebSettings}` ->
   `{@link WebSettings}`). However, **never modify URLs** (like `https://...` or
   other web links) inside comments.
6. **Nested and Inner Classes:** For inline references to nested or inner
   classes (e.g. `org.chromium.chrome.browser.profiles.ProfileKey.Theme`),
   prefer importing the top-level outer class
   (`import org.chromium.chrome.browser.profiles.ProfileKey;`) and referring to
   it as `ProfileKey.Theme` in the code, rather than importing the nested class
   directly.
   - *Exception:* If importing the nested class directly is the dominant style
     in that specific file, match the existing style.
7. **Apply Replacements:**
   - Replace the inline FQN with its simple class name.
   - Add the corresponding `import` statement at the top of the file.

### Step 4: Validation

1. **Code Formatting:** Execute `git cl format` to format the modified source
   code and organize imports. Address any errors that are reported.
2. **Mandatory Final Review:** Follow the
   [Automated Review Protocol](references/automated_review.md) to delegate a
   final review of the patch to the `generalist` sub-agent. Proceed to the
   Verification phase only after the review returns `PASS`.

### Step 5: Verification

Follow the [Verification](../hub/references/verification.md) workflow.

### Step 6: Submission

Invoke the [Submission](../hub/references/submission.md) workflow. Pass the
following context variables to the workflow:

- **Skill Name:** `java-inline-fqn-cleanup`
- **Branch Name:** `cleanup-fqns-[component-name]`
- **Commit Hashtag:** `Code Health`
- **Cleanup Title:** `Clean up inline FQNs in [Component/Directory]`
- **Cleanup Description:**
  `Remove inline fully qualified class names and replace them with standard imports in the [Component/Directory] directory.`
- **Parent Bug:** `528570333`
- **Cleaned Component:** The parent directory of the batch.
- **File Count:** Number of files cleaned up.
