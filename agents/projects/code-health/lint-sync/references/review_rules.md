# Review Rules: Lint Sync Guards

- **Strict Scope:** Focus ONLY on the LINT guards that were added in this
  specific patch. Do NOT suggest adding guards to other pre-existing unguarded
  enums in the file.
- **Data Integrity:** If missing entries are identified in the XML, the
  remediation MUST ONLY suggest appending them. Never suggest modifying or
  removing existing names or values in either the source or the XML.
- **Syntax & Placement:** Verify that the `LINT.IfChange` and `LINT.ThenChange`
  guards are placed correctly before and after the enum definitions in both the
  source code and the XML file. Verify that labels match exactly and paths are
  valid (blank lines between the guards and the code they protect are
  acceptable).
- **Consistency & Style:** Ensure that legacy manual synchronization comments
  (e.g., "keep in sync with enums.xml") have been removed, but all other related
  comments have been preserved.
