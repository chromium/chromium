# Ruleset Segmentation Guidance

To maintain high reasoning quality and avoid context dilution, persona
checklists must remain small and focused.

## The 10-Item Rule

No single persona checklist should exceed **10 items**. If adding a new rule
causes the checklist to exceed this limit, the ruleset MUST be segmented.

## Segmentation Strategy

1. **Identify Sub-Domains:** Divide the checklist items into logical
   sub-categories (e.g., in a `security.json` persona, split into
   `ipc_security.json` and `input_validation_security.json`).
2. **Create Sub-Personas:** Create new persona JSON files for these sub-domains
   in a nested directory structure (up to 5 levels deep).
3. **Update Routing:** Update the target skill's `ROUTING.md` to map specific
   file patterns or complexity flags to these new sub-personas instead of the
   original parent persona.
4. **Clean up Parent:** Remove the migrated checklist items from the parent
   persona.
