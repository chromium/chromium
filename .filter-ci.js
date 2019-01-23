const childprocess = require('child_process');

String.prototype.endWith = function (endStr) {
  let d = this.length - endStr.length;
  return (d >= 0 && this.lastIndexOf(endStr) == d);
};
function checkURL(url) {
  let reg = (/(((https?:(?:\/\/)?)(?:[-;:&=\+\$,\w]+@)?[A-Za-z0-9.-]+|(?:www.|[-;:&=\+\$,\w]+@)[A-Za-z0-9.-]+)((?:\/[\+~%\/.\w-_]*)?\??(?:[-\+=&;%@.\w_]*)#?(?:[\w]*))?)/g);
  if (!reg.test(url)) {
    return false;
  } else {
    return true;
  }
}

let commands;
commands = 'git log -1 ' + process.argv.slice(2) + ' --pretty="%cE"';
// check submit email standard.
childprocess.exec(commands, function (err, stdout, stderr) {
  if (err != null) {
    console.log('command fail, error message : ' , err);
  }
  stdout = stdout.toLowerCase();
  if (stdout.match('intel')) {
    if (!(stdout.replace(/[\r\n]/g, '').endWith('@intel.com'))) {
      console.log('Please configure the correct author of email of your git before contributing code.');
      console.log('Submit email addr is : ', stdout);
      process.exit(1);
    }
  }
});

commands = 'git log -1 ' + process.argv.slice(2) + ' --pretty="%s"';
// check submit explain whether contains sensitive fields or network addr.
childprocess.exec(commands, function (err, stdout, stderr) {
  if (err != null) {
    console.log('command fail, error message : ' , err);
  }
  stdout = stdout.toLowerCase();
  if (stdout.match('intel')) {
    console.log('Please make sure commit that contains sensitive fields. \nCommit content is : ', stdout);
    process.exit(1);
  }
  if (checkURL(stdout)) {
    console.log('Please make sure is a sensitive network address. \nCommit content is : ', stdout);
    process.exit(1);
  }
});
