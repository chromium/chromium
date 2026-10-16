---
name: general/avoid-broad-addpatchset-in-project-config
rubric: safety
severity: blocking
---

**Rule.** When configuring Gerrit repository access controls in project.config,
do not grant addPatchSet permissions to administrative or automation groups
across broad branch references (such as refs/heads/\*). Restrict CL management
and automation tooling to least-privilege permissions such as abandon and
removeReviewer.

**Why.** Granting addPatchSet on broad branch references allows members or
service accounts of that group to upload arbitrary new patch sets to any open CL
across the repository regardless of change ownership. Following least privilege
prevents unintended privilege escalation and unauthorized code modifications
across user CLs.

## Examples

Bad:

```cpp
[access "refs/heads/*"]
	abandon = group mdb/peeps-genai-team-admin
	addPatchSet = group mdb/peeps-genai-team-admin
```

Good:

```cpp
[access "refs/heads/*"]
	abandon = group mdb/peeps-genai-team-admin
	removeReviewer = group mdb/peeps-genai-team-admin
```
