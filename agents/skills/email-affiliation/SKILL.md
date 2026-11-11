---
name: email-affiliation
description: >-
  Resolves a contributor email address and optional date to JSON containing its
  affiliated domain and friendly organization name (or 'Individual' /
  'Undisclosed') using Chromium's affiliations.json5 and org-list.txt files. Use
  when determining which company or organization a Chromium contributor, commit
  author, or reviewer is or was affiliated with.
---

# Contributor Email Affiliation Lookup

Resolves an email address (currently or on a given `YYYY-MM-DD` date) to a JSON
object `{"domain": <str|null>, "organization": <str|null>}` using
[`affiliations.json5`](/third_party/blink/tools/commit_stats/affiliations.json5)
and [`org-list.txt`](/third_party/blink/tools/commit_stats/org-list.txt).

## Usage

Run [email_affiliation.py](scripts/email_affiliation.py) from the Chromium `src`
directory:

```bash
# Current affiliation (defaults to today's local date):
vpython3 agents/skills/email-affiliation/scripts/email_affiliation.py \
  user@chromium.org

# Affiliation on a specific date (e.g., a commit's %as author date):
vpython3 agents/skills/email-affiliation/scripts/email_affiliation.py \
  schenney@chromium.org 2020-06-01
```

Prints single-line JSON to `stdout`, for example:

- `{"domain": "microsoft.com", "organization": "Microsoft"}`
- `{"domain": "gmail.com", "organization": "Individual"}`
- `{"domain": null, "organization": "Undisclosed"}`
- `{"domain": "citrolabs.ai", "organization": null}` (domain not mapped to a
  known organization in `org-list.txt`)

Exits non-zero on invalid email or date arguments.

## Resolution Rules

1. Look up `<email>` (case-insensitive) in
   `third_party/blink/tools/commit_stats/affiliations.json5`. If listed, scan
   `affiliations` (ordered most recent `start` first) for the first entry with
   `start <= date`:
   - If its `domain` is `Individual` or `Undisclosed`, return `domain: null` and
     that value as `organization`.
   - Otherwise use that lowercase `domain` for step 3.
   - If `date` precedes every listed `start` date for that account, return
     `{"domain": null, "organization": "Undisclosed"}`.
2. If unlisted in `affiliations.json5`, resolve `domain` to `google.com` for
   `@chromium.org` addresses and the email's lowercase domain otherwise.
3. Look up `domain` in `third_party/blink/tools/commit_stats/org-list.txt`: map
   `Unknown` or unmapped domains to `null`, and return all other friendly
   organization names verbatim.

## Caveats

- **Do not consult [`AUTHORS`](/AUTHORS) to infer affiliation**: people listed
  in the `individuals` section of `AUTHORS` may still be contributing on behalf
  of their organization (for example, when a corporate CLA authorizes specific
  employees rather than a `*@domain` wildcard).
- **Best-effort accuracy**: because this data is derived primarily from
  self-attested and public information, results may not be 100% accurate.

## Testing

```bash
vpython3 agents/skills/email-affiliation/scripts/email_affiliation_unittest.py
```
