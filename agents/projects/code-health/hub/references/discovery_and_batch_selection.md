# Discovery & Batch Selection

Follow this workflow to identify candidates to process.

## Steps

1. **Discovery:** Run the unified discovery script `candidate_finder.py` and
   pass the path to the skill's plugin script:

   ```bash
   python3 agents/projects/code-health/hub/scripts/candidate_finder.py find \
     --plugin agents/projects/code-health/<skill_name>/scripts/find_candidates.py \
     [--count N] \
     [--json] \
     [--exclude <names_or_paths> ...] \
     [--exclude-file <path>] \
     [--sort random|asc|desc]
   ```

   - `--json` emits structured candidate JSON.
   - `--exclude` / `--exclude-file` skip the given candidate names, file paths,
     or JSON ledger entries.
   - `--sort asc|desc` orders results alphabetically by directory path (atomic
     plugins only; grouped plugins reject `asc`/`desc`).

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
   batch". If the user rejects the batch, re-run discovery with
   `--exclude <rejected_paths_or_names>` appended. Keep all prior rejections in
   `--exclude` across retries so rejected items are not re-selected.
