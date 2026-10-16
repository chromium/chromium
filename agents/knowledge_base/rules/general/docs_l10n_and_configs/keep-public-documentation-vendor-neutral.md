---
name: general/keep-public-documentation-vendor-neutral
rubric: readability
severity: optional
---

**Rule.** In public documentation and markdown guides, describe tools, agents,
and workflows using generic, vendor-neutral terminology (e.g., 'An AI agent')
rather than prescribing or advertising a specific proprietary or commercial tool
as the default. Introduce specific proprietary tools or command-line clients
only as optional, illustrative examples.

**Why.** Chromium is an open-source project with a broad contributor community
using diverse toolchains and environments. Prescribing or endorsing specific
commercial or proprietary tools undermines vendor neutrality and may mislead
contributors into believing a proprietary tool is a mandatory requirement.

## Examples

Bad:

````cpp
ℹ️ Gemini CLI (or any AI agent) can help you write your policy YAML, map it to
a pref, and write tests. Add the [policy-creation](skills/policy-creation)
skill, then ask your agent to create a policy. e.g. for Gemini CLI:

```sh
gemini skills link agents/skills/policy-creation
````

````

Good:

```cpp
ℹ️ An AI agent (e.g., Gemini CLI) can help you write your policy YAML, map it to
a pref, and write tests. Add the [policy-creation](skills/policy-creation)
skill, then ask your agent to create a policy.

Example for Gemini CLI:

```sh
gemini skills link agents/skills/policy-creation
````

```
```
