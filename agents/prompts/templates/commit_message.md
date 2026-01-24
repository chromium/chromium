# Instructions for Generating a Chromium Commit Message

As an expert Chromium developer and Gerrit specialist, your goal is to analyze the current session and generate a useful Change List (CL) description. You understand that a Chromium commit message is a permanent record of technical rationale and a trigger for automated infrastructure.

### 1. Pre-flight Investigation (Interactivity)
Before generating the final draft, analyze the session history. If any of the following are missing or ambiguous, **STOP and ask the user for clarification**:
*   **The "Why":** If the technical rationale or motivation isn't explicitly clear from the session history.
*   **Bug ID:** If no bug number was mentioned, ask if one should be associated.
*   **Internal vs. Public Bug:** If a bug ID is present, confirm if it's a public Chromium bug or an internal Buganizer issue.
*   **Manual Testing:** If no test commands were successfully run, ask the user how they verified the change to populate the `Test:` footer.

### 2. Formatting Constraints (Mandatory)
*   **72-Column Wrap:** Every line (Subject and Body) **MUST** be hard-wrapped at 72 characters.
*   **Subject Line:** A single, concise summary. Prefix it with the relevant component in brackets, e.g., `[Omnibox]: ...`.
*   **Subject Spacing:** There **MUST** be exactly one blank line after the subject.
*   **Footer Spacing:** There should be no blank lines within the footer block.

### 3. Body Content Requirements
*   **Content over Code:** Do not just list what changed. Focus on **why** it was necessary.
*   **Context:** Describe the "Before" (the problem/baseline) and the "After" (the solution/new behavior).
*   **Reference Links:** Use the format `https://crrev.com/c/NUMBER` for Gerrit CL references.

### 4. Critical Footer Logic
*   **Internal Bugs (Buganizer):** MUST use the `b:` prefix. Example: `Bug: b:123456`.
    *   **WARNING:** Never use the `b/123` format; it triggers OSS lint warnings.
*   **Public Bugs:** Use the bare number. Example: `Bug: 123456`.
*   **Closing Bugs:** Use the `Fixed:` tag if the bug should be closed automatically.
*   **Verification:** Populate the `Test:` footer with manual verification steps or the specific test suites run.

---

## Final Message Template:

```
[Component]: [Short summary of change]

[Description explaining the "Why" and "How". Focus on rationale,
previous behavior, and the impact of the change. Wrap this
block strictly at 72 characters.]

Bug: [b:ID or ID]
Test: [Manual test commands or verification steps]
```
