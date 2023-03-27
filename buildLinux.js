// Script used by buildkite to build Chromium for Linux in CI

const path = require("path");
const { spawnSync } = require("child_process");

const chromium = process.cwd();

const branch = process.env["BUILDKITE_BRANCH"];

syncRepo(chromium, `origin/${branch}`);

const deps = getChromiumDeps();

syncRepo(path.join(chromium, "v8"), deps.v8);

syncRepo(path.join(chromium, "third_party", "skia"), deps.skia);

syncRepo(path.join(chromium, "third_party", "webrtc"), deps.webrtc);

syncRepo(path.join(chromium, "third_party", "boringssl", "src"), deps.boringssl);

const dockerArgs = [
  "run",
  "-e",
  "GOMA_SERVER_HOST=simpsonite.goma.engflow.com",
  "-e",
  "GOMACTL_USE_PROXY=false",
  "-e",
  "DRIVER_REVISION",
  "-v",
  `${path.join(process.env.HOME, "chromium")}:/chromium`,
  "-v",
  `${path.join(process.env.HOME, "depot_tools")}:/depot_tools`,
  "-v",
  `${path.join(
    process.env.HOME,
    ".goma_client_oauth2_config"
  )}:/home/ubuntu/.goma_client_oauth2_config`,
  "-p",
  "9098:9099",
  "chromium-build-new",
];

spawnChecked("docker", dockerArgs, { stdio: "inherit" });

function spawnChecked(cmd, args, options) {
  const prettyCmd = [cmd].concat(args).join(" ");
  console.error(prettyCmd);

  const rv = spawnSync(cmd, args, options);

  if (rv.status != 0 || rv.error) {
    console.error("Process failed:", rv.error || "");
    console.log(rv.stdout.toString() || "");
    console.error(rv.stderr.toString() || "");
    throw new Error(`Spawned process failed with exit code ${rv.status}`);
  }

  return rv;
}

function syncRepo(dir, treeish) {
  try {
    spawnChecked("git", ["fetch", "--all"], { cwd: dir, stdio: "inherit" });
  } catch (e) {
    // Ignore errors due to being at a detached head.
  }

  spawnChecked("git", ["reset", "--hard", treeish], { cwd: dir, stdio: "inherit" });
}

function getChromiumDeps() {
  const text = fs.readFileSync("DEPS", "utf8");
  let results = {
    v8: "",
    skia: "",
    webrtc: "",
    boringssl: ""
  };

  let match = /'v8_revision': '(.*?)'/.exec(text);
  assert(match, "Could not find V8 revision");
  results.v8 = match;

  match = /'skia_revision': '(.*?)'/.exec(text);
  assert(match, "Could not find skia revision");
  results.skia = match;

  match = /'https:\/\/github.com\/replayio\/chromium-webrtc.git' \+ '@' \+ '(.*?)'/.exec(
    text
  );
  assert(match, "Could not find webrtc revision");
  results.webrtc = match;

  match = /'boringssl_revision': '(.*?)'/.exec(text);
  assert(match, "Could not find boringssl revision");
  results.boringssl = match;

  return results;
}