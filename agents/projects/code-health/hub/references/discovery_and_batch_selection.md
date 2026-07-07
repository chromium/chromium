# Discovery & Batch Selection

Follow this workflow to identify candidates to process.

## Steps

1. **Discovery:** Run the discovery script `find_candidates.py` located in the
   skill's `scripts/` directory:

   ```bash
   python3 agents/projects/code-health/<skill_name>/scripts/find_candidates.py
   ```

2. **Present Candidates/Batch:** Output the candidate details or candidate batch
   to the user. Format the output to list:

   - **Batch Directory / File Path:** [Directory or File]
   - **File / Candidate Count:** [Count]
   - **Files / Candidate Details:** \[List of files or details such as Name,
     Owners, Expiry, Source/XML Paths, Issue Lines, Reason, etc.\]
   - **\[Optional Skill-Specific Metadata\]:** (Include any additional relevant
     details, e.g., imports found, warnings, banned symbols, etc.)

3. **Proceed Confirmation:** Prompt the user using `ask_question`. The choices
   must include the package name for the "Yes" action and "No, discard this
   batch". If the user rejects the batch, repeat this workflow to find the next
   candidate batch while excluding the rejected paths.
