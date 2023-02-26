For internal-only documentation, see http://shortn/_M1K9wX8pM3.

Wilco is a family of ChromeOS laptops have have integration with Dell
SupportAssist. Dell supplies a sandboxed VM (wilco_dtc) which has access to the
system through a first party system daemon (wilco_dtc_supportd). Dell also
supplies a dynamically generated chrome app (not via chrome web store).
This directory serves as the glue code. It's functionality includes:
  * Bridging communication between the daemon and chrome app.
  * Providing network communication facilities to the sandboxed VM.
  * Supplying configuration via device policy.
