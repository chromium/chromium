document.addEventListener('DOMContentLoaded', function () {
  const loadModelBERTButton = document.getElementById('loadModelBERT');
  const inferSingleButton = document.getElementById('inferSingleButton');
  const inferBatchButton = document.getElementById('inferBatchButton');
  const benchmarkInferenceButton = document.getElementById('benchmarkInferenceButton');
  const trainModelButton = document.getElementById('trainModel');
  
  // Inputs for single inference.
  const questionInput = document.getElementById('questionInput');
  const contextInput = document.getElementById('contextInput');
  
  // Input for batch inference (expects a JSON array).
  const batchInput = document.getElementById('batchInput');
  
  // Load MobileBERT Model.
  if (loadModelBERTButton) {
    loadModelBERTButton.addEventListener('click', () => {
      chrome.readServer.loadModelBERT((response) => {
        try {
          const parsedResponse = JSON.parse(response);
          if (parsedResponse.status) {
            console.log('Model loaded:', parsedResponse.status);
            alert('Model loaded: ' + parsedResponse.status);
          } else {
            console.error('Error:', parsedResponse.error);
            alert('Error: ' + parsedResponse.error);
          }
        } catch (e) {
          console.error('Failed to parse load model response:', e);
          // alert('Error: Failed to parse load model response');
        }
      });
    });
  } else {
    console.error("Button with ID 'loadModelBERT' not found in DOM.");
  }
  
  // Single Inference.
  if (inferSingleButton && questionInput && contextInput) {
    inferSingleButton.addEventListener('click', () => {
      const question = questionInput.value.trim();
      const context = contextInput.value.trim();
      if (!question || !context) {
        alert('Please provide both a question and a context.');
        return;
      }
      const payload = { question: question, context: context };
      const jsonPayload = JSON.stringify(payload);
      console.log("Single inference payload:", jsonPayload);
      chrome.readServer.inferSingleBERT(jsonPayload, (response) => {
        try {
          const parsedResponse = JSON.parse(response);
          if (parsedResponse.answer) {
            console.log('Single inference result:', parsedResponse.answer);
            alert('Answer: ' + parsedResponse.answer);
          } else {
            console.error('Error:', parsedResponse.error);
            alert('Error: ' + parsedResponse.error);
          }
        } catch (e) {
          console.error('Failed to parse single inference response:', e);
          alert('Error: Failed to parse inference response');
        }
      });
    });
  } else {
    console.error("Single inference elements not found in DOM.");
  }
  
  // Batch Inference.
  if (inferBatchButton && batchInput) {
    inferBatchButton.addEventListener('click', () => {
      let batchData;
      try {
        batchData = JSON.parse(batchInput.value.trim());
      } catch (e) {
        alert('Invalid JSON for batch input. Please provide a valid JSON array.');
        return;
      }
      const jsonPayload = JSON.stringify(batchData);
      console.log("Batch inference payload:", jsonPayload);
      chrome.readServer.inferBatchBERT(jsonPayload, (response) => {
        try {
          const parsedResponse = JSON.parse(response);
          console.log('Batch inference result:', parsedResponse);
          alert('Batch Inference Result: ' + JSON.stringify(parsedResponse));
        } catch (e) {
          console.error('Failed to parse batch inference response:', e);
          alert('Error: Failed to parse batch inference response');
        }
      });
    });
  } else {
    console.error("Batch inference elements not found in DOM.");
  }
  
  // Benchmark Inference.
  if (benchmarkInferenceButton) {
    benchmarkInferenceButton.addEventListener('click', async () => {
      // Define fixed question and context 
      const question = "What day was the game played on?";
      const context = "The game was played on February 7, 2016 at Levi's Stadium in the San Francisco Bay Area at Santa Clara, California.";
      const payload = JSON.stringify({ question, context });
      
      for (let i = 0; i < 100; i++) {
        await new Promise(resolve => {
          chrome.readServer.inferSingleBERT(payload, () => resolve());
        });
      }
      
      // Measurement: run a fixed number of iterations.
      const iterations = 1000; 
      let latencies = [];
      
      for (let i = 0; i < iterations; i++) {
        const startTime = performance.now();
        await new Promise(resolve => {
          chrome.readServer.inferSingleBERT(payload, () => resolve());
        });
        const endTime = performance.now();
        latencies.push(endTime - startTime);
      }
      
      // Compute statistics.
      latencies.sort((a, b) => a - b);
      const sum = latencies.reduce((acc, cur) => acc + cur, 0);
      const avg = sum / latencies.length;
      const median = latencies[Math.floor(latencies.length / 2)];
      const p90 = latencies[Math.floor(latencies.length * 0.9)];
      
      console.log("Benchmark results:");
      console.log(`Average latency: ${avg.toFixed(2)} ms`);
      console.log(`Median latency: ${median.toFixed(2)} ms`);
      console.log(`90th percentile latency: ${p90.toFixed(2)} ms`);
      
      alert(`Benchmark over ${iterations} iterations:\nAverage: ${avg.toFixed(2)} ms\nMedian: ${median.toFixed(2)} ms\n90th Percentile: ${p90.toFixed(2)} ms`);
    });
  } else {
    console.error("Button with ID 'benchmarkInferenceButton' not found in DOM.");
  }
  
  // Training with client-side timing
  if (trainModelButton) {
    trainModelButton.addEventListener('click', () => {
      const startTime = performance.now(); // Start timing here
      chrome.readServer.trainModel((response) => {
        const endTime = performance.now(); // End timing when response is received
        const clientTime = endTime - startTime;

        try {
          const parsedResponse = JSON.parse(response);
          if (parsedResponse.training_time_ms && parsedResponse.accuracy) {
            console.log('Training result:', parsedResponse);
            // Display both client-side and backend timing
            alert(`Training completed:\nClient-side Time: ${clientTime.toFixed(2)} ms\nBackend Time: ${parsedResponse.training_time_ms.toFixed(2)} ms\nAccuracy: ${(parsedResponse.accuracy * 100).toFixed(2)}%`);
          } else {
            console.error('Error:', parsedResponse.error);
            alert('Error: ' + parsedResponse.error);
          }
        } catch (e) {
          console.error('Failed to parse training response:', e);
          alert('Error: Failed to parse training response');
        }
      });
    });
  } else {
    console.error("Button with ID 'trainModel' not found in DOM.");
  }
});

