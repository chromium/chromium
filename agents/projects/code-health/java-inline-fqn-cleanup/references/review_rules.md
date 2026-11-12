# Review Rules: Java Inline FQN Cleanup

- **Collision Avoidance:** Verify that replacing an inline FQN (e.g., `a.b.C`)
  did not introduce a collision with another class `C` imported from a different
  package (e.g., `x.y.C`).
- **Completeness:** Verify that all occurrences of the inline FQN in the
  modified files have been replaced with the simple class name.
- **No Redundant Imports:** Verify that no `java.lang.*` classes were explicitly
  imported, as these are implicitly imported by Java and will fail linters.
- **Preserved Resource Classes:** Verify that no FQNs containing `.R.` (e.g.,
  `android.R.attr`, `com.example.R.id`) were modified or imported. They MUST
  remain fully qualified inline to prevent naming collisions with the local `R`
  class.
